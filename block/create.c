/*
 * Block layer code related to image creation
 *
 * Copyright (c) 2018 Kevin Wolf <kwolf@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "block/block_int.h"
#include "qemu/job.h"
#include "qapi/qapi-commands-block-core.h"
#include "qapi/qapi-visit-block-core.h"
#include "qapi/clone-visitor.h"
#include "qapi/error.h"

typedef struct BlockdevCreateJob {
    Job common;
    BlockDriver *drv;
    BlockdevCreateOptions *opts;
    int ret;
    Error *err;
} BlockdevCreateJob;

static void blockdev_create_complete(Job *job, void *opaque)
{
    BlockdevCreateJob *s = container_of(job, BlockdevCreateJob, common);

    job_completed(job, s->ret, s->err);
}

static void coroutine_fn blockdev_create_run(void *opaque)
{
    BlockdevCreateJob *s = opaque;

    job_progress_set_remaining(&s->common, 1);
    s->ret = s->drv->bdrv_co_create(s->opts, &s->err);
    job_progress_update(&s->common, 1);

    qapi_free_BlockdevCreateOptions(s->opts);
    job_defer_to_main_loop(&s->common, blockdev_create_complete, NULL);
}

static const JobDriver blockdev_create_job_driver = {
    .instance_size = sizeof(BlockdevCreateJob),
    .job_type      = JOB_TYPE_CREATE,
    .start         = blockdev_create_run,
};

void qmp_blockdev_create(const char *job_id, BlockdevCreateOptions *options,
                         Error **errp)
{
    BlockdevCreateJob *s;
    const char *fmt = BlockdevDriver_str(options->driver);
    BlockDriver *drv = bdrv_find_format(fmt);

    /* If the driver is in the schema, we know that it exists. But it may not
     * be whitelisted. */
    assert(drv);
    if (bdrv_uses_whitelist() && !bdrv_is_whitelisted(drv, false)) {
        error_setg(errp, "Driver is not whitelisted");
        return;
    }

    /* Error out if the driver doesn't support .bdrv_co_create */
    if (!drv->bdrv_co_create) {
        error_setg(errp, "Driver does not support blockdev-create");
        return;
    }

    /* Create the block job */
    /* TODO Running in the main context. Block drivers need to error out or add
     * locking when they use a BDS in a different AioContext. */
    s = job_create(job_id, &blockdev_create_job_driver, NULL,
                   qemu_get_aio_context(), JOB_DEFAULT | JOB_MANUAL_DISMISS,
                   NULL, NULL, errp);
    if (!s) {
        return;
    }

    s->drv = drv,
    s->opts = QAPI_CLONE(BlockdevCreateOptions, options),

    job_start(&s->common);
}
