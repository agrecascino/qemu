/*
 * Copyright (C) 2014       Citrix Systems UK Ltd.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "hw/xen/xen_backend.h"
#include "qmp-commands.h"
#include "sysemu/char.h"

//#define DEBUG_XEN

#ifdef DEBUG_XEN
#define DPRINTF(fmt, ...) \
    do { fprintf(stderr, "xen: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

static int store_dev_info(int domid, CharDriverState *cs, const char *string)
{
    struct xs_handle *xs = NULL;
    char *path = NULL;
    char *newpath = NULL;
    char *pts = NULL;
    int ret = -1;

    /* Only continue if we're talking to a pty. */
    if (strncmp(cs->filename, "pty:", 4)) {
        return 0;
    }
    pts = cs->filename + 4;

    /* We now have everything we need to set the xenstore entry. */
    xs = xs_open(0);
    if (xs == NULL) {
        fprintf(stderr, "Could not contact XenStore\n");
        goto out;
    }

    path = xs_get_domain_path(xs, domid);
    if (path == NULL) {
        fprintf(stderr, "xs_get_domain_path() error\n");
        goto out;
    }
    newpath = realloc(path, (strlen(path) + strlen(string) +
                strlen("/tty") + 1));
    if (newpath == NULL) {
        fprintf(stderr, "realloc error\n");
        goto out;
    }
    path = newpath;

    strcat(path, string);
    strcat(path, "/tty");
    if (!xs_write(xs, XBT_NULL, path, pts, strlen(pts))) {
        fprintf(stderr, "xs_write for '%s' fail", string);
        goto out;
    }
    ret = 0;

out:
    free(path);
    xs_close(xs);

    return ret;
}

void xenstore_store_pv_console_info(int i, CharDriverState *chr)
{
    if (i == 0) {
        store_dev_info(xen_domid, chr, "/console");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "/device/console/%d", i);
        store_dev_info(xen_domid, chr, buf);
    }
}


static void xenstore_record_dm_state(struct xs_handle *xs, const char *state)
{
    char path[50];

    if (xs == NULL) {
        fprintf(stderr, "xenstore connection not initialized\n");
        exit(1);
    }

    snprintf(path, sizeof (path), "device-model/%u/state", xen_domid);
    if (!xs_write(xs, XBT_NULL, path, state, strlen(state))) {
        fprintf(stderr, "error recording dm state\n");
        exit(1);
    }
}


static void xen_change_state_handler(void *opaque, int running,
                                     RunState state)
{
    if (running) {
        /* record state running */
        xenstore_record_dm_state(xenstore, "running");
    }
}

int xen_init(MachineClass *mc)
{
    xen_xc = xen_xc_interface_open(0, 0, 0);
    if (xen_xc == XC_HANDLER_INITIAL_VALUE) {
        xen_be_printf(NULL, 0, "can't open xen interface\n");
        return -1;
    }
    qemu_add_vm_change_state_handler(xen_change_state_handler, NULL);

    return 0;
}

