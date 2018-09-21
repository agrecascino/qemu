/*
 * QEMU KVM Hyper-V support
 *
 * Copyright (C) 2015 Andrey Smetanin <asmetanin@virtuozzo.com>
 *
 * Authors:
 *  Andrey Smetanin <asmetanin@virtuozzo.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef TARGET_I386_HYPERV_H
#define TARGET_I386_HYPERV_H

#include "cpu.h"
#include "sysemu/kvm.h"
#include "hw/hyperv/hyperv.h"

int kvm_hv_handle_exit(X86CPU *cpu, struct kvm_hyperv_exit *exit);

#endif
