/*
 *  Dynamic device configuration and creation.
 *
 *  Copyright (c) 2009 CodeSourcery
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/* The theory here is that it should be possible to create a machine without
   knowledge of specific devices.  Historically board init routines have
   passed a bunch of arguments to each device, requiring the board know
   exactly which device it is dealing with.  This file provides an abstract
   API for device configuration and initialization.  Devices will generally
   inherit from a particular bus (e.g. PCI or I2C) rather than
   this API directly.  */

#include "net.h"
#include "qdev.h"
#include "sysemu.h"
#include "monitor.h"

struct DeviceProperty {
    const char *name;
    DevicePropType type;
    union {
        uint64_t i;
        void *ptr;
    } value;
    DeviceProperty *next;
};

/* This is a nasty hack to allow passing a NULL bus to qdev_create.  */
static BusState *main_system_bus;
extern struct BusInfo system_bus_info;

static DeviceInfo *device_info_list;

/* Register a new device type.  */
void qdev_register(DeviceInfo *info)
{
    assert(info->size >= sizeof(DeviceState));
    assert(!info->next);

    info->next = device_info_list;
    device_info_list = info;
}

/* Create a new device.  This only initializes the device state structure
   and allows properties to be set.  qdev_init should be called to
   initialize the actual device emulation.  */
DeviceState *qdev_create(BusState *bus, const char *name)
{
    DeviceInfo *info;
    DeviceState *dev;

    if (!bus) {
        if (!main_system_bus) {
            main_system_bus = qbus_create(&system_bus_info, NULL, "main-system-bus");
        }
        bus = main_system_bus;
    }

    for (info = device_info_list; info != NULL; info = info->next) {
        if (info->bus_info != bus->info)
            continue;
        if (strcmp(info->name, name) != 0)
            continue;
        break;
    }
    if (!info) {
        hw_error("Unknown device '%s' for bus '%s'\n", name, bus->info->name);
    }

    dev = qemu_mallocz(info->size);
    dev->info = info;
    dev->parent_bus = bus;
    LIST_INSERT_HEAD(&bus->children, dev, sibling);
    return dev;
}

/* Initialize a device.  Device properties should be set before calling
   this function.  IRQs and MMIO regions should be connected/mapped after
   calling this function.  */
void qdev_init(DeviceState *dev)
{
    dev->info->init(dev, dev->info);
}

/* Unlink device from bus and free the structure.  */
void qdev_free(DeviceState *dev)
{
    LIST_REMOVE(dev, sibling);
    free(dev);
}

static DeviceProperty *create_prop(DeviceState *dev, const char *name,
                                   DevicePropType type)
{
    DeviceProperty *prop;

    /* TODO: Check for duplicate properties.  */
    prop = qemu_mallocz(sizeof(*prop));
    prop->name = qemu_strdup(name);
    prop->type = type;
    prop->next = dev->props;
    dev->props = prop;

    return prop;
}

void qdev_set_prop_int(DeviceState *dev, const char *name, uint64_t value)
{
    DeviceProperty *prop;

    prop = create_prop(dev, name, PROP_TYPE_INT);
    prop->value.i = value;
}

void qdev_set_prop_dev(DeviceState *dev, const char *name, DeviceState *value)
{
    DeviceProperty *prop;

    prop = create_prop(dev, name, PROP_TYPE_DEV);
    prop->value.ptr = value;
}

void qdev_set_prop_ptr(DeviceState *dev, const char *name, void *value)
{
    DeviceProperty *prop;

    prop = create_prop(dev, name, PROP_TYPE_PTR);
    prop->value.ptr = value;
}

void qdev_set_netdev(DeviceState *dev, NICInfo *nd)
{
    assert(!dev->nd);
    dev->nd = nd;
}


/* Get a character (serial) device interface.  */
CharDriverState *qdev_init_chardev(DeviceState *dev)
{
    static int next_serial;
    static int next_virtconsole;
    /* FIXME: This is a nasty hack that needs to go away.  */
    if (strncmp(dev->info->name, "virtio", 6) == 0) {
        return virtcon_hds[next_virtconsole++];
    } else {
        return serial_hds[next_serial++];
    }
}

BusState *qdev_get_parent_bus(DeviceState *dev)
{
    return dev->parent_bus;
}

static DeviceProperty *find_prop(DeviceState *dev, const char *name,
                                 DevicePropType type)
{
    DeviceProperty *prop;

    for (prop = dev->props; prop; prop = prop->next) {
        if (strcmp(prop->name, name) == 0) {
            assert (prop->type == type);
            return prop;
        }
    }
    return NULL;
}

uint64_t qdev_get_prop_int(DeviceState *dev, const char *name, uint64_t def)
{
    DeviceProperty *prop;

    prop = find_prop(dev, name, PROP_TYPE_INT);
    if (!prop) {
        return def;
    }

    return prop->value.i;
}

void *qdev_get_prop_ptr(DeviceState *dev, const char *name)
{
    DeviceProperty *prop;

    prop = find_prop(dev, name, PROP_TYPE_PTR);
    assert(prop);
    return prop->value.ptr;
}

DeviceState *qdev_get_prop_dev(DeviceState *dev, const char *name)
{
    DeviceProperty *prop;

    prop = find_prop(dev, name, PROP_TYPE_DEV);
    if (!prop) {
        return NULL;
    }
    return prop->value.ptr;
}

void qdev_init_gpio_in(DeviceState *dev, qemu_irq_handler handler, int n)
{
    assert(dev->num_gpio_in == 0);
    dev->num_gpio_in = n;
    dev->gpio_in = qemu_allocate_irqs(handler, dev, n);
}

void qdev_init_gpio_out(DeviceState *dev, qemu_irq *pins, int n)
{
    assert(dev->num_gpio_out == 0);
    dev->num_gpio_out = n;
    dev->gpio_out = pins;
}

qemu_irq qdev_get_gpio_in(DeviceState *dev, int n)
{
    assert(n >= 0 && n < dev->num_gpio_in);
    return dev->gpio_in[n];
}

void qdev_connect_gpio_out(DeviceState * dev, int n, qemu_irq pin)
{
    assert(n >= 0 && n < dev->num_gpio_out);
    dev->gpio_out[n] = pin;
}

VLANClientState *qdev_get_vlan_client(DeviceState *dev,
                                      NetCanReceive *can_receive,
                                      NetReceive *receive,
                                      NetReceiveIOV *receive_iov,
                                      NetCleanup *cleanup,
                                      void *opaque)
{
    NICInfo *nd = dev->nd;
    assert(nd);
    nd->vc = qemu_new_vlan_client(nd->vlan, nd->model, nd->name, can_receive,
                                  receive, receive_iov, cleanup, opaque);
    return nd->vc;
}


void qdev_get_macaddr(DeviceState *dev, uint8_t *macaddr)
{
    memcpy(macaddr, dev->nd->macaddr, 6);
}

static int next_block_unit[IF_COUNT];

/* Get a block device.  This should only be used for single-drive devices
   (e.g. SD/Floppy/MTD).  Multi-disk devices (scsi/ide) should use the
   appropriate bus.  */
BlockDriverState *qdev_init_bdrv(DeviceState *dev, BlockInterfaceType type)
{
    int unit = next_block_unit[type]++;
    int index;

    index = drive_get_index(type, 0, unit);
    if (index == -1) {
        return NULL;
    }
    return drives_table[index].bdrv;
}

BusState *qdev_get_child_bus(DeviceState *dev, const char *name)
{
    BusState *bus;

    LIST_FOREACH(bus, &dev->child_bus, sibling) {
        if (strcmp(name, bus->name) == 0) {
            return bus;
        }
    }
    return NULL;
}

static int next_scsi_bus;

/* Create a scsi bus, and attach devices to it.  */
/* TODO: Actually create a scsi bus for hotplug to use.  */
void scsi_bus_new(DeviceState *host, SCSIAttachFn attach)
{
   int bus = next_scsi_bus++;
   int unit;
   int index;

   for (unit = 0; unit < MAX_SCSI_DEVS; unit++) {
       index = drive_get_index(IF_SCSI, bus, unit);
       if (index == -1) {
           continue;
       }
       attach(host, drives_table[index].bdrv, unit);
   }
}

BusState *qbus_create(BusInfo *info, DeviceState *parent, const char *name)
{
    BusState *bus;

    bus = qemu_mallocz(info->size);
    bus->info = info;
    bus->parent = parent;
    bus->name = qemu_strdup(name);
    LIST_INIT(&bus->children);
    if (parent) {
        LIST_INSERT_HEAD(&parent->child_bus, bus, sibling);
    }
    return bus;
}

#define qdev_printf(fmt, ...) monitor_printf(mon, "%*s" fmt, indent, "", ## __VA_ARGS__)
static void qbus_print(Monitor *mon, BusState *bus, int indent);

static void qdev_print(Monitor *mon, DeviceState *dev, int indent)
{
    DeviceProperty *prop;
    BusState *child;
    qdev_printf("dev: %s\n", dev->info->name);
    indent += 2;
    if (dev->num_gpio_in) {
        qdev_printf("gpio-in %d\n", dev->num_gpio_in);
    }
    if (dev->num_gpio_out) {
        qdev_printf("gpio-out %d\n", dev->num_gpio_out);
    }
    for (prop = dev->props; prop; prop = prop->next) {
        switch (prop->type) {
        case PROP_TYPE_INT:
            qdev_printf("prop-int %s 0x%" PRIx64 "\n", prop->name,
                        prop->value.i);
            break;
        case PROP_TYPE_PTR:
            qdev_printf("prop-ptr %s\n", prop->name);
            break;
        case PROP_TYPE_DEV:
            qdev_printf("prop-dev %s %s\n", prop->name,
                        ((DeviceState *)prop->value.ptr)->info->name);
            break;
        default:
            qdev_printf("prop-unknown%d %s\n", prop->type, prop->name);
            break;
        }
    }
    if (dev->parent_bus->info->print_dev)
        dev->parent_bus->info->print_dev(mon, dev, indent);
    LIST_FOREACH(child, &dev->child_bus, sibling) {
        qbus_print(mon, child, indent);
    }
}

static void qbus_print(Monitor *mon, BusState *bus, int indent)
{
    struct DeviceState *dev;

    qdev_printf("bus: %s\n", bus->name);
    indent += 2;
    qdev_printf("type %s\n", bus->info->name);
    LIST_FOREACH(dev, &bus->children, sibling) {
        qdev_print(mon, dev, indent);
    }
}
#undef qdev_printf

void do_info_qtree(Monitor *mon)
{
    if (main_system_bus)
        qbus_print(mon, main_system_bus, 0);
}
