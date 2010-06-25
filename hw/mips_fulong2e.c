/*
 * QEMU fulong 2e mini pc support
 *
 * Copyright (c) 2008 yajin (yajin@vm-kernel.org)
 * Copyright (c) 2009 chenming (chenming@rdc.faw.com.cn)
 * Copyright (c) 2010 Huacai Chen (zltjiangshi@gmail.com)
 * This code is licensed under the GNU GPL v2.
 */

/*
 * Fulong 2e mini pc is based on ICT/ST Loongson 2e CPU (MIPS III like, 800MHz)
 * http://www.linux-mips.org/wiki/Fulong
 *
 * Loongson 2e user manual:
 * http://www.loongsondeveloper.com/doc/Loongson2EUserGuide.pdf
 */

#include "hw.h"
#include "pc.h"
#include "fdc.h"
#include "net.h"
#include "boards.h"
#include "smbus.h"
#include "block.h"
#include "flash.h"
#include "mips.h"
#include "mips_cpudevs.h"
#include "pci.h"
#include "usb-uhci.h"
#include "qemu-char.h"
#include "sysemu.h"
#include "audio/audio.h"
#include "qemu-log.h"
#include "loader.h"
#include "mips-bios.h"
#include "ide.h"
#include "elf.h"
#include "vt82c686.h"
#include "mc146818rtc.h"

#define DEBUG_FULONG2E_INIT

#define ENVP_ADDR       0x80002000l
#define ENVP_NB_ENTRIES	 	16
#define ENVP_ENTRY_SIZE	 	256

#define MAX_IDE_BUS 2

/*
 * PMON is not part of qemu and released with BSD license, anyone
 * who want to build a pmon binary please first git-clone the source
 * from the git repository at:
 * http://www.loongson.cn/support/git/pmon
 * Then follow the "Compile Guide" available at:
 * http://dev.lemote.com/code/pmon
 *
 * Notes:
 * 1, don't use the source at http://dev.lemote.com/http_git/pmon.git
 * 2, use "Bonito2edev" to replace "dir_corresponding_to_your_target_hardware"
 * in the "Compile Guide".
 */
#define FULONG_BIOSNAME "pmon_fulong2e.bin"

/* PCI SLOT in fulong 2e */
#define FULONG2E_VIA_SLOT        5
#define FULONG2E_ATI_SLOT        6
#define FULONG2E_RTL8139_SLOT    7

static PITState *pit;

static struct _loaderparams {
    int ram_size;
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
} loaderparams;

static void prom_set(uint32_t* prom_buf, int index, const char *string, ...)
{
    va_list ap;
    int32_t table_addr;

    if (index >= ENVP_NB_ENTRIES)
        return;

    if (string == NULL) {
        prom_buf[index] = 0;
        return;
    }

    table_addr = sizeof(int32_t) * ENVP_NB_ENTRIES + index * ENVP_ENTRY_SIZE;
    prom_buf[index] = tswap32(ENVP_ADDR + table_addr);

    va_start(ap, string);
    vsnprintf((char *)prom_buf + table_addr, ENVP_ENTRY_SIZE, string, ap);
    va_end(ap);
}

static int64_t load_kernel (CPUState *env)
{
    int64_t kernel_entry, kernel_low, kernel_high;
    int index = 0;
    long initrd_size;
    ram_addr_t initrd_offset;
    uint32_t *prom_buf;
    long prom_size;

    if (load_elf(loaderparams.kernel_filename, cpu_mips_kseg0_to_phys, NULL,
                 (uint64_t *)&kernel_entry, (uint64_t *)&kernel_low,
                 (uint64_t *)&kernel_high, 0, ELF_MACHINE, 1) < 0) {
        fprintf(stderr, "qemu: could not load kernel '%s'\n",
                loaderparams.kernel_filename);
        exit(1);
    }

    /* load initrd */
    initrd_size = 0;
    initrd_offset = 0;
    if (loaderparams.initrd_filename) {
        initrd_size = get_image_size (loaderparams.initrd_filename);
        if (initrd_size > 0) {
            initrd_offset = (kernel_high + ~TARGET_PAGE_MASK) & TARGET_PAGE_MASK;
            if (initrd_offset + initrd_size > ram_size) {
                fprintf(stderr,
                        "qemu: memory too small for initial ram disk '%s'\n",
                        loaderparams.initrd_filename);
                exit(1);
            }
            initrd_size = load_image_targphys(loaderparams.initrd_filename,
                                     initrd_offset, ram_size - initrd_offset);
        }
        if (initrd_size == (target_ulong) -1) {
            fprintf(stderr, "qemu: could not load initial ram disk '%s'\n",
                    loaderparams.initrd_filename);
            exit(1);
        }
    }

    /* Setup prom parameters. */
    prom_size = ENVP_NB_ENTRIES * (sizeof(int32_t) + ENVP_ENTRY_SIZE);
    prom_buf = qemu_malloc(prom_size);

    prom_set(prom_buf, index++, loaderparams.kernel_filename);
    if (initrd_size > 0) {
        prom_set(prom_buf, index++, "rd_start=0x" PRIx64 " rd_size=%li %s",
                 cpu_mips_phys_to_kseg0(NULL, initrd_offset), initrd_size,
                 loaderparams.kernel_cmdline);
    } else {
        prom_set(prom_buf, index++, loaderparams.kernel_cmdline);
    }

    /* Setup minimum environment variables */
    prom_set(prom_buf, index++, "busclock=33000000");
    prom_set(prom_buf, index++, "cpuclock=100000000");
    prom_set(prom_buf, index++, "memsize=%i", loaderparams.ram_size/1024/1024);
    prom_set(prom_buf, index++, "modetty0=38400n8r");
    prom_set(prom_buf, index++, NULL);

    rom_add_blob_fixed("prom", prom_buf, prom_size,
                       cpu_mips_kseg0_to_phys(NULL, ENVP_ADDR));

    return kernel_entry;
}

static void write_bootloader (CPUState *env, uint8_t *base, int64_t kernel_addr)
{
    uint32_t *p;

    /* Small bootloader */
    p = (uint32_t *) base;

    stl_raw(p++, 0x0bf00010);                                      /* j 0x1fc00040 */
    stl_raw(p++, 0x00000000);                                      /* nop */

    /* Second part of the bootloader */
    p = (uint32_t *) (base + 0x040);

    stl_raw(p++, 0x3c040000);                                      /* lui a0, 0 */
    stl_raw(p++, 0x34840002);                                      /* ori a0, a0, 2 */
    stl_raw(p++, 0x3c050000 | ((ENVP_ADDR >> 16) & 0xffff));       /* lui a1, high(ENVP_ADDR) */
    stl_raw(p++, 0x34a50000 | (ENVP_ADDR & 0xffff));               /* ori a1, a0, low(ENVP_ADDR) */
    stl_raw(p++, 0x3c060000 | (((ENVP_ADDR + 8) >> 16) & 0xffff)); /* lui a2, high(ENVP_ADDR + 8) */
    stl_raw(p++, 0x34c60000 | ((ENVP_ADDR + 8) & 0xffff));         /* ori a2, a2, low(ENVP_ADDR + 8) */
    stl_raw(p++, 0x3c070000 | (loaderparams.ram_size >> 16));      /* lui a3, high(env->ram_size) */
    stl_raw(p++, 0x34e70000 | (loaderparams.ram_size & 0xffff));   /* ori a3, a3, low(env->ram_size) */
    stl_raw(p++, 0x3c1f0000 | ((kernel_addr >> 16) & 0xffff));     /* lui ra, high(kernel_addr) */;
    stl_raw(p++, 0x37ff0000 | (kernel_addr & 0xffff));             /* ori ra, ra, low(kernel_addr) */
    stl_raw(p++, 0x03e00008);                                      /* jr ra */
    stl_raw(p++, 0x00000000);                                      /* nop */
}


static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;

    cpu_reset(env);
    /* TODO: 2E reset stuff */
    if (loaderparams.kernel_filename) {
        env->CP0_Status &= ~((1 << CP0St_BEV) | (1 << CP0St_ERL));
    }
}

uint8_t eeprom_spd[0x80] = {
    0x80,0x08,0x07,0x0d,0x09,0x02,0x40,0x00,0x04,0x70,
    0x70,0x00,0x82,0x10,0x00,0x01,0x0e,0x04,0x0c,0x01,
    0x02,0x20,0x80,0x75,0x70,0x00,0x00,0x50,0x3c,0x50,
    0x2d,0x20,0xb0,0xb0,0x50,0x50,0x00,0x00,0x00,0x00,
    0x00,0x41,0x48,0x3c,0x32,0x75,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x9c,0x7b,0x07,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x48,0x42,0x35,0x34,0x41,0x32,
    0x35,0x36,0x38,0x4b,0x4e,0x2d,0x41,0x37,0x35,0x42,
    0x20,0x30,0x20
};

/* Audio support */
#ifdef HAS_AUDIO
static void audio_init (PCIBus *pci_bus)
{
    vt82c686b_ac97_init(pci_bus, (FULONG2E_VIA_SLOT << 3) + 5);
    vt82c686b_mc97_init(pci_bus, (FULONG2E_VIA_SLOT << 3) + 6);
}
#endif

/* Network support */
static void network_init (void)
{
    int i;

    for(i = 0; i < nb_nics; i++) {
        NICInfo *nd = &nd_table[i];
        const char *default_devaddr = NULL;

        if (i == 0 && (!nd->model || strcmp(nd->model, "rtl8139") == 0)) {
            /* The fulong board has a RTL8139 card using PCI SLOT 7 */
            default_devaddr = "07";
        }

        pci_nic_init_nofail(nd, "rtl8139", default_devaddr);
    }
}

static void cpu_request_exit(void *opaque, int irq, int level)
{
    CPUState *env = cpu_single_env;

    if (env && level) {
        cpu_exit(env);
    }
}

static void mips_fulong2e_init(ram_addr_t ram_size, const char *boot_device,
                        const char *kernel_filename, const char *kernel_cmdline,
                        const char *initrd_filename, const char *cpu_model)
{
    char *filename;
    unsigned long ram_offset, bios_offset;
    unsigned long bios_size;
    int64_t kernel_entry;
    qemu_irq *i8259;
    qemu_irq *cpu_exit_irq;
    int via_devfn;
    PCIBus *pci_bus;
    ISADevice *isa_dev;
    uint8_t *eeprom_buf;
    i2c_bus *smbus;
    int i;
    DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    DeviceState *eeprom;
    ISADevice *rtc_state;
    CPUState *env;

    /* init CPUs */
    if (cpu_model == NULL) {
        cpu_model = "Loongson-2E";
    }
    env = cpu_init(cpu_model);
    if (!env) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    register_savevm(NULL, "cpu", 0, 3, cpu_save, cpu_load, env);
    qemu_register_reset(main_cpu_reset, env);

    /* fulong 2e has 256M ram. */
    ram_size = 256 * 1024 * 1024;

    /* fulong 2e has a 1M flash.Winbond W39L040AP70Z */
    bios_size = 1024 * 1024;

    /* allocate RAM */
    ram_offset = qemu_ram_alloc(NULL, "fulong2e.ram", ram_size);
    bios_offset = qemu_ram_alloc(NULL, "fulong2e.bios", bios_size);

    cpu_register_physical_memory(0, ram_size, IO_MEM_RAM);
    cpu_register_physical_memory(0x1fc00000LL,
					   bios_size, bios_offset | IO_MEM_ROM);

    /* We do not support flash operation, just loading pmon.bin as raw BIOS.
     * Please use -L to set the BIOS path and -bios to set bios name. */

    if (kernel_filename) {
        loaderparams.ram_size = ram_size;
        loaderparams.kernel_filename = kernel_filename;
        loaderparams.kernel_cmdline = kernel_cmdline;
        loaderparams.initrd_filename = initrd_filename;
        kernel_entry = load_kernel (env);
        write_bootloader(env, qemu_get_ram_ptr(bios_offset), kernel_entry);
    } else {
        if (bios_name == NULL) {
                bios_name = FULONG_BIOSNAME;
        }
        filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
        if (filename) {
            bios_size = load_image_targphys(filename, 0x1fc00000LL,
                                            BIOS_SIZE);
            qemu_free(filename);
        } else {
            bios_size = -1;
        }

        if ((bios_size < 0 || bios_size > BIOS_SIZE) && !kernel_filename) {
            fprintf(stderr, "qemu: Could not load MIPS bios '%s'\n", bios_name);
            exit(1);
        }
    }

    /* Init internal devices */
    cpu_mips_irq_init_cpu(env);
    cpu_mips_clock_init(env);

    /* Interrupt controller */
    /* The 8259 -> IP5  */
    i8259 = i8259_init(env->irq[5]);

    /* North bridge, Bonito --> IP2 */
    pci_bus = bonito_init((qemu_irq *)&(env->irq[2]));

    /* South bridge */
    if (drive_get_max_bus(IF_IDE) >= MAX_IDE_BUS) {
        fprintf(stderr, "qemu: too many IDE bus\n");
        exit(1);
    }

    for(i = 0; i < MAX_IDE_BUS * MAX_IDE_DEVS; i++) {
        hd[i] = drive_get(IF_IDE, i / MAX_IDE_DEVS, i % MAX_IDE_DEVS);
    }

    via_devfn = vt82c686b_init(pci_bus, FULONG2E_VIA_SLOT << 3);
    if (via_devfn < 0) {
        fprintf(stderr, "vt82c686b_init error \n");
        exit(1);
    }

    isa_bus_irqs(i8259);
    vt82c686b_ide_init(pci_bus, hd, (FULONG2E_VIA_SLOT << 3) + 1);
    usb_uhci_vt82c686b_init(pci_bus, (FULONG2E_VIA_SLOT << 3) + 2);
    usb_uhci_vt82c686b_init(pci_bus, (FULONG2E_VIA_SLOT << 3) + 3);

    smbus = vt82c686b_pm_init(pci_bus, (FULONG2E_VIA_SLOT << 3) + 4,
                              0xeee1, NULL);
    eeprom_buf = qemu_mallocz(8 * 256); /* XXX: make this persistent */
    memcpy(eeprom_buf, eeprom_spd, sizeof(eeprom_spd));
    /* TODO: Populate SPD eeprom data.  */
    eeprom = qdev_create((BusState *)smbus, "smbus-eeprom");
    qdev_prop_set_uint8(eeprom, "address", 0x50);
    qdev_prop_set_ptr(eeprom, "data", eeprom_buf);
    qdev_init_nofail(eeprom);

    /* init other devices */
    pit = pit_init(0x40, isa_reserve_irq(0));
    cpu_exit_irq = qemu_allocate_irqs(cpu_request_exit, NULL, 1);
    DMA_init(0, cpu_exit_irq);

    /* Super I/O */
    isa_dev = isa_create_simple("i8042");

    rtc_state = rtc_init(2000, NULL);

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        if (serial_hds[i]) {
            serial_isa_init(i, serial_hds[i]);
        }
    }

    if (parallel_hds[0]) {
        parallel_init(0, parallel_hds[0]);
    }

    /* Sound card */
#ifdef HAS_AUDIO
    audio_init(pci_bus);
#endif
    /* Network card */
    network_init();
}

QEMUMachine mips_fulong2e_machine = {
    .name = "fulong2e",
    .desc = "Fulong 2e mini pc",
    .init = mips_fulong2e_init,
};

static void mips_fulong2e_machine_init(void)
{
    qemu_register_machine(&mips_fulong2e_machine);
}

machine_init(mips_fulong2e_machine_init);
