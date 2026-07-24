/*
 * BCM2712 minimal peripheral emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/bcm2712_peripherals.h"
#include "hw/core/qdev-properties.h"
#include "hw/misc/unimp.h"
#include "system/system.h"

#define BCM2712_UART_BASE       0x107d001000ULL
#define BCM2712_SYSTIMER_BASE   0x107c003000ULL
#define BCM2712_SDHCI_BASE      0x1000fff000ULL
#define BCM2712_SDHCI_CAPAREG   0x00000000052134b4ULL
#define BCM2712_MBOX_BASE       0x107c013880ULL
#define BCM2712_MBOX_SIZE       0x40
#define BCM2712_L2_INTC_BASE    0x107d517000ULL
#define BCM2712_L2_INTC_SIZE    0x2000

static void bcm2712_peripherals_init(Object *obj)
{
    BCM2712PeripheralState *s = BCM2712_PERIPHERALS(obj);

    object_initialize_child(obj, "uart", &s->uart, TYPE_PL011);
    object_initialize_child(obj, "systimer", &s->systimer,
                            TYPE_BCM2835_SYSTIMER);
    object_initialize_child(obj, "sdhci", &s->sdhci, TYPE_SYSBUS_SDHCI);
}

static void bcm2712_peripherals_realize(DeviceState *dev, Error **errp)
{
    BCM2712PeripheralState *s = BCM2712_PERIPHERALS(dev);

    create_unimplemented_device("bcm2712-mbox", BCM2712_MBOX_BASE,
                                BCM2712_MBOX_SIZE);
    create_unimplemented_device("bcm2712-l2-intc", BCM2712_L2_INTC_BASE,
                                BCM2712_L2_INTC_SIZE);

    qdev_prop_set_chr(DEVICE(&s->uart), "chardev", serial_hd(0));
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart), 0, BCM2712_UART_BASE);

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->systimer), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->systimer), 0, BCM2712_SYSTIMER_BASE);

    object_property_set_uint(OBJECT(&s->sdhci), "sd-spec-version", 3,
                             &error_abort);
    object_property_set_uint(OBJECT(&s->sdhci), "capareg",
                             BCM2712_SDHCI_CAPAREG, &error_abort);
    object_property_set_bool(OBJECT(&s->sdhci), "pending-insert-quirk", true,
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->sdhci), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sdhci), 0, BCM2712_SDHCI_BASE);
    object_property_add_alias(OBJECT(dev), "sd-bus",
                              OBJECT(&s->sdhci), "sd-bus");
}

static void bcm2712_peripherals_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = bcm2712_peripherals_realize;
}

static const TypeInfo bcm2712_peripherals_type_info = {
    .name = TYPE_BCM2712_PERIPHERALS,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2712PeripheralState),
    .instance_init = bcm2712_peripherals_init,
    .class_init = bcm2712_peripherals_class_init,
};

static void bcm2712_peripherals_register_types(void)
{
    type_register_static(&bcm2712_peripherals_type_info);
}

type_init(bcm2712_peripherals_register_types)
