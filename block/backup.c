/*
 * QEMU backup
 *
 * Copyright (C) 2013 Proxmox Server Solutions
 *
 * Authors:
 *  Dietmar Maurer (dietmar@proxmox.com)
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"

#include "trace.h"
#include "block/block.h"
#include "block/block_int.h"
#include "block/blockjob_int.h"
#include "block/block_backup.h"
#include "block/block-copy.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qemu/ratelimit.h"
#include "qemu/cutils.h"
#include "sysemu/block-backend.h"
#include "qemu/bitmap.h"
#include "qemu/error-report.h"

#define BACKUP_CLUSTER_SIZE_DEFAULT (1 << 16)

typedef struct BackupBlockJob {
    BlockJob common;
    BlockDriverState *source_bs;

    BdrvDirtyBitmap *sync_bitmap;

    MirrorSyncMode sync_mode;
    BitmapSyncMode bitmap_mode;
    BlockdevOnError on_source_error;
    BlockdevOnError on_target_error;
    CoRwlock flush_rwlock;
    uint64_t len;
    uint64_t bytes_read;
    int64_t cluster_size;
    NotifierWithReturn before_write;

    BlockCopyState *bcs;
} BackupBlockJob;

static const BlockJobDriver backup_job_driver;

static void backup_progress_bytes_callback(int64_t bytes, void *opaque)
{
    BackupBlockJob *s = opaque;

    s->bytes_read += bytes;
    job_progress_update(&s->common.job, bytes);
}

static void backup_progress_reset_callback(void *opaque)
{
    BackupBlockJob *s = opaque;
    uint64_t estimate = bdrv_get_dirty_count(s->bcs->copy_bitmap);

    job_progress_set_remaining(&s->common.job, estimate);
}

static int coroutine_fn backup_do_cow(BackupBlockJob *job,
                                      int64_t offset, uint64_t bytes,
                                      bool *error_is_read,
                                      bool is_write_notifier)
{
    int ret = 0;
    int64_t start, end; /* bytes */

    qemu_co_rwlock_rdlock(&job->flush_rwlock);

    start = QEMU_ALIGN_DOWN(offset, job->cluster_size);
    end = QEMU_ALIGN_UP(bytes + offset, job->cluster_size);

    trace_backup_do_cow_enter(job, start, offset, bytes);

    ret = block_copy(job->bcs, start, end - start, error_is_read,
                     is_write_notifier);

    trace_backup_do_cow_return(job, offset, bytes, ret);

    qemu_co_rwlock_unlock(&job->flush_rwlock);

    return ret;
}

static int coroutine_fn backup_before_write_notify(
        NotifierWithReturn *notifier,
        void *opaque)
{
    BackupBlockJob *job = container_of(notifier, BackupBlockJob, before_write);
    BdrvTrackedRequest *req = opaque;

    assert(req->bs == job->source_bs);
    assert(QEMU_IS_ALIGNED(req->offset, BDRV_SECTOR_SIZE));
    assert(QEMU_IS_ALIGNED(req->bytes, BDRV_SECTOR_SIZE));

    return backup_do_cow(job, req->offset, req->bytes, NULL, true);
}

static void backup_cleanup_sync_bitmap(BackupBlockJob *job, int ret)
{
    BdrvDirtyBitmap *bm;
    bool sync = (((ret == 0) || (job->bitmap_mode == BITMAP_SYNC_MODE_ALWAYS)) \
                 && (job->bitmap_mode != BITMAP_SYNC_MODE_NEVER));

    if (sync) {
        /*
         * We succeeded, or we always intended to sync the bitmap.
         * Delete this bitmap and install the child.
         */
        bm = bdrv_dirty_bitmap_abdicate(job->source_bs, job->sync_bitmap, NULL);
    } else {
        /*
         * We failed, or we never intended to sync the bitmap anyway.
         * Merge the successor back into the parent, keeping all data.
         */
        bm = bdrv_reclaim_dirty_bitmap(job->source_bs, job->sync_bitmap, NULL);
    }

    assert(bm);

    if (ret < 0 && job->bitmap_mode == BITMAP_SYNC_MODE_ALWAYS) {
        /* If we failed and synced, merge in the bits we didn't copy: */
        bdrv_dirty_bitmap_merge_internal(bm, job->bcs->copy_bitmap,
                                         NULL, true);
    }
}

static void backup_commit(Job *job)
{
    BackupBlockJob *s = container_of(job, BackupBlockJob, common.job);
    if (s->sync_bitmap) {
        backup_cleanup_sync_bitmap(s, 0);
    }
}

static void backup_abort(Job *job)
{
    BackupBlockJob *s = container_of(job, BackupBlockJob, common.job);
    if (s->sync_bitmap) {
        backup_cleanup_sync_bitmap(s, -1);
    }
}

static void backup_clean(Job *job)
{
    BackupBlockJob *s = container_of(job, BackupBlockJob, common.job);

    block_copy_state_free(s->bcs);
}

void backup_do_checkpoint(BlockJob *job, Error **errp)
{
    BackupBlockJob *backup_job = container_of(job, BackupBlockJob, common);

    assert(block_job_driver(job) == &backup_job_driver);

    if (backup_job->sync_mode != MIRROR_SYNC_MODE_NONE) {
        error_setg(errp, "The backup job only supports block checkpoint in"
                   " sync=none mode");
        return;
    }

    bdrv_set_dirty_bitmap(backup_job->bcs->copy_bitmap, 0, backup_job->len);
}

static BlockErrorAction backup_error_action(BackupBlockJob *job,
                                            bool read, int error)
{
    if (read) {
        return block_job_error_action(&job->common, job->on_source_error,
                                      true, error);
    } else {
        return block_job_error_action(&job->common, job->on_target_error,
                                      false, error);
    }
}

static bool coroutine_fn yield_and_check(BackupBlockJob *job)
{
    uint64_t delay_ns;

    if (job_is_cancelled(&job->common.job)) {
        return true;
    }

    /*
     * We need to yield even for delay_ns = 0 so that bdrv_drain_all() can
     * return. Without a yield, the VM would not reboot.
     */
    delay_ns = block_job_ratelimit_get_delay(&job->common, job->bytes_read);
    job->bytes_read = 0;
    job_sleep_ns(&job->common.job, delay_ns);

    if (job_is_cancelled(&job->common.job)) {
        return true;
    }

    return false;
}

static int coroutine_fn backup_loop(BackupBlockJob *job)
{
    bool error_is_read;
    int64_t offset;
    BdrvDirtyBitmapIter *bdbi;
    int ret = 0;

    bdbi = bdrv_dirty_iter_new(job->bcs->copy_bitmap);
    while ((offset = bdrv_dirty_iter_next(bdbi)) != -1) {
        do {
            if (yield_and_check(job)) {
                goto out;
            }
            ret = backup_do_cow(job, offset,
                                job->cluster_size, &error_is_read, false);
            if (ret < 0 && backup_error_action(job, error_is_read, -ret) ==
                           BLOCK_ERROR_ACTION_REPORT)
            {
                goto out;
            }
        } while (ret < 0);
    }

 out:
    bdrv_dirty_iter_free(bdbi);
    return ret;
}

static void backup_init_copy_bitmap(BackupBlockJob *job)
{
    bool ret;
    uint64_t estimate;

    if (job->sync_mode == MIRROR_SYNC_MODE_BITMAP) {
        ret = bdrv_dirty_bitmap_merge_internal(job->bcs->copy_bitmap,
                                               job->sync_bitmap,
                                               NULL, true);
        assert(ret);
    } else {
        if (job->sync_mode == MIRROR_SYNC_MODE_TOP) {
            /*
             * We can't hog the coroutine to initialize this thoroughly.
             * Set a flag and resume work when we are able to yield safely.
             */
            job->bcs->skip_unallocated = true;
        }
        bdrv_set_dirty_bitmap(job->bcs->copy_bitmap, 0, job->len);
    }

    estimate = bdrv_get_dirty_count(job->bcs->copy_bitmap);
    job_progress_set_remaining(&job->common.job, estimate);
}

static int coroutine_fn backup_run(Job *job, Error **errp)
{
    BackupBlockJob *s = container_of(job, BackupBlockJob, common.job);
    int ret = 0;

    qemu_co_rwlock_init(&s->flush_rwlock);

    backup_init_copy_bitmap(s);

    s->before_write.notify = backup_before_write_notify;
    bdrv_add_before_write_notifier(s->source_bs, &s->before_write);

    if (s->sync_mode == MIRROR_SYNC_MODE_TOP) {
        int64_t offset = 0;
        int64_t count;

        for (offset = 0; offset < s->len; ) {
            if (yield_and_check(s)) {
                ret = -ECANCELED;
                goto out;
            }

            ret = block_copy_reset_unallocated(s->bcs, offset, &count);
            if (ret < 0) {
                goto out;
            }

            offset += count;
        }
        s->bcs->skip_unallocated = false;
    }

    if (s->sync_mode == MIRROR_SYNC_MODE_NONE) {
        /*
         * All bits are set in copy_bitmap to allow any cluster to be copied.
         * This does not actually require them to be copied.
         */
        while (!job_is_cancelled(job)) {
            /*
             * Yield until the job is cancelled.  We just let our before_write
             * notify callback service CoW requests.
             */
            job_yield(job);
        }
    } else {
        ret = backup_loop(s);
    }

 out:
    notifier_with_return_remove(&s->before_write);

    /* wait until pending backup_do_cow() calls have completed */
    qemu_co_rwlock_wrlock(&s->flush_rwlock);
    qemu_co_rwlock_unlock(&s->flush_rwlock);

    return ret;
}

static const BlockJobDriver backup_job_driver = {
    .job_driver = {
        .instance_size          = sizeof(BackupBlockJob),
        .job_type               = JOB_TYPE_BACKUP,
        .free                   = block_job_free,
        .user_resume            = block_job_user_resume,
        .run                    = backup_run,
        .commit                 = backup_commit,
        .abort                  = backup_abort,
        .clean                  = backup_clean,
    }
};

static int64_t backup_calculate_cluster_size(BlockDriverState *target,
                                             Error **errp)
{
    int ret;
    BlockDriverInfo bdi;

    /*
     * If there is no backing file on the target, we cannot rely on COW if our
     * backup cluster size is smaller than the target cluster size. Even for
     * targets with a backing file, try to avoid COW if possible.
     */
    ret = bdrv_get_info(target, &bdi);
    if (ret == -ENOTSUP && !target->backing) {
        /* Cluster size is not defined */
        warn_report("The target block device doesn't provide "
                    "information about the block size and it doesn't have a "
                    "backing file. The default block size of %u bytes is "
                    "used. If the actual block size of the target exceeds "
                    "this default, the backup may be unusable",
                    BACKUP_CLUSTER_SIZE_DEFAULT);
        return BACKUP_CLUSTER_SIZE_DEFAULT;
    } else if (ret < 0 && !target->backing) {
        error_setg_errno(errp, -ret,
            "Couldn't determine the cluster size of the target image, "
            "which has no backing file");
        error_append_hint(errp,
            "Aborting, since this may create an unusable destination image\n");
        return ret;
    } else if (ret < 0 && target->backing) {
        /* Not fatal; just trudge on ahead. */
        return BACKUP_CLUSTER_SIZE_DEFAULT;
    }

    return MAX(BACKUP_CLUSTER_SIZE_DEFAULT, bdi.cluster_size);
}

BlockJob *backup_job_create(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *target, int64_t speed,
                  MirrorSyncMode sync_mode, BdrvDirtyBitmap *sync_bitmap,
                  BitmapSyncMode bitmap_mode,
                  bool compress,
                  BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  int creation_flags,
                  BlockCompletionFunc *cb, void *opaque,
                  JobTxn *txn, Error **errp)
{
    int64_t len;
    BackupBlockJob *job = NULL;
    int64_t cluster_size;
    BdrvRequestFlags write_flags;

    assert(bs);
    assert(target);

    /* QMP interface protects us from these cases */
    assert(sync_mode != MIRROR_SYNC_MODE_INCREMENTAL);
    assert(sync_bitmap || sync_mode != MIRROR_SYNC_MODE_BITMAP);

    if (bs == target) {
        error_setg(errp, "Source and target cannot be the same");
        return NULL;
    }

    if (!bdrv_is_inserted(bs)) {
        error_setg(errp, "Device is not inserted: %s",
                   bdrv_get_device_name(bs));
        return NULL;
    }

    if (!bdrv_is_inserted(target)) {
        error_setg(errp, "Device is not inserted: %s",
                   bdrv_get_device_name(target));
        return NULL;
    }

    if (compress && !block_driver_can_compress(target->drv)) {
        error_setg(errp, "Compression is not supported for this drive %s",
                   bdrv_get_device_name(target));
        return NULL;
    }

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_BACKUP_SOURCE, errp)) {
        return NULL;
    }

    if (bdrv_op_is_blocked(target, BLOCK_OP_TYPE_BACKUP_TARGET, errp)) {
        return NULL;
    }

    if (sync_bitmap) {
        /* If we need to write to this bitmap, check that we can: */
        if (bitmap_mode != BITMAP_SYNC_MODE_NEVER &&
            bdrv_dirty_bitmap_check(sync_bitmap, BDRV_BITMAP_DEFAULT, errp)) {
            return NULL;
        }

        /* Create a new bitmap, and freeze/disable this one. */
        if (bdrv_dirty_bitmap_create_successor(bs, sync_bitmap, errp) < 0) {
            return NULL;
        }
    }

    len = bdrv_getlength(bs);
    if (len < 0) {
        error_setg_errno(errp, -len, "unable to get length for '%s'",
                         bdrv_get_device_name(bs));
        goto error;
    }

    cluster_size = backup_calculate_cluster_size(target, errp);
    if (cluster_size < 0) {
        goto error;
    }

    /*
     * If source is in backing chain of target assume that target is going to be
     * used for "image fleecing", i.e. it should represent a kind of snapshot of
     * source at backup-start point in time. And target is going to be read by
     * somebody (for example, used as NBD export) during backup job.
     *
     * In this case, we need to add BDRV_REQ_SERIALISING write flag to avoid
     * intersection of backup writes and third party reads from target,
     * otherwise reading from target we may occasionally read already updated by
     * guest data.
     *
     * For more information see commit f8d59dfb40bb and test
     * tests/qemu-iotests/222
     */
    write_flags = (bdrv_chain_contains(target, bs) ? BDRV_REQ_SERIALISING : 0) |
                  (compress ? BDRV_REQ_WRITE_COMPRESSED : 0),

    /* job->len is fixed, so we can't allow resize */
    job = block_job_create(job_id, &backup_job_driver, txn, bs, 0, BLK_PERM_ALL,
                           speed, creation_flags, cb, opaque, errp);
    if (!job) {
        goto error;
    }

    job->source_bs = bs;
    job->on_source_error = on_source_error;
    job->on_target_error = on_target_error;
    job->sync_mode = sync_mode;
    job->sync_bitmap = sync_bitmap;
    job->bitmap_mode = bitmap_mode;

    job->bcs = block_copy_state_new(bs, target, cluster_size, write_flags,
                                    errp);
    if (!job->bcs) {
        goto error;
    }

    job->cluster_size = cluster_size;
    job->len = len;

    block_copy_set_callbacks(job->bcs, backup_progress_bytes_callback,
                             backup_progress_reset_callback, job);

    /* Required permissions are already taken by block-copy-state target */
    block_job_add_bdrv(&job->common, "target", target, 0, BLK_PERM_ALL,
                       &error_abort);

    return &job->common;

 error:
    if (sync_bitmap) {
        bdrv_reclaim_dirty_bitmap(bs, sync_bitmap, NULL);
    }
    if (job) {
        backup_clean(&job->common.job);
        job_early_fail(&job->common.job);
    }

    return NULL;
}
