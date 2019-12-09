/*
 * QEMU i440FX North Bridge Emulation
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef HW_PCI_I440FX_H
#define HW_PCI_I440FX_H

#include "hw/hw.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci-host/pam.h"

#define TYPE_I440FX_PCI_HOST_BRIDGE "i440FX-pcihost"
#define TYPE_I440FX_PCI_DEVICE "i440FX"

#define I440FX_PCI_DEVICE(obj) \
    OBJECT_CHECK(PCII440FXState, (obj), TYPE_I440FX_PCI_DEVICE)

typedef struct PCII440FXState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    MemoryRegion *system_memory;
    MemoryRegion *pci_address_space;
    MemoryRegion *ram_memory;
    PAMMemoryRegion pam_regions[13];
    MemoryRegion smram_region;
    MemoryRegion smram, low_smram;
} PCII440FXState;

#define TYPE_IGD_PASSTHROUGH_I440FX_PCI_DEVICE "igd-passthrough-i440FX"

PCIBus *i440fx_init(const char *host_type, const char *pci_type,
                    PCII440FXState **pi440fx_state,
                    MemoryRegion *address_space_mem,
                    MemoryRegion *address_space_io,
                    ram_addr_t ram_size,
                    ram_addr_t below_4g_mem_size,
                    ram_addr_t above_4g_mem_size,
                    MemoryRegion *pci_memory,
                    MemoryRegion *ram_memory);

PCIBus *find_i440fx(void);

#endif
