/*
 * BCM2712 SoC emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/arm/bcm2712.h"
#include "hw/core/sysbus.h"

#define BCM2712_GIC_DIST_BASE       0x107fff9000ULL
#define BCM2712_GIC_CPU_BASE        0x107fffa000ULL
#define BCM2712_GIC_VIFACE_BASE     0x107fffc000ULL
#define BCM2712_GIC_VCPU_BASE       0x107fffe000ULL

#define BCM2712_GIC_NUM_SPIS        320
#define BCM2712_GIC_INTERNAL        32
#define BCM2712_GIC_UART_IRQ        121
#define BCM2712_GIC_SYSTIMER_IRQ    64
#define BCM2712_GIC_SDHCI_IRQ       273

#define GIC400_MAINTENANCE_IRQ      9
#define GIC400_TIMER_NS_EL2_IRQ     10
#define GIC400_TIMER_VIRT_IRQ       11
#define GIC400_TIMER_S_EL1_IRQ      13
#define GIC400_TIMER_NS_EL1_IRQ     14
#define VIRTUAL_PMU_IRQ             7

#define PPI(cpu, irq) \
    (BCM2712_GIC_NUM_SPIS + (cpu) * BCM2712_GIC_INTERNAL + 16 + (irq))

static void bcm2712_init(Object *obj)
{
    BCM2712State *s = BCM2712(obj);

    for (unsigned int n = 0; n < BCM2712_NUM_CPUS; n++) {
        object_initialize_child(obj, "cpu[*]", &s->cpu[n],
                                ARM_CPU_TYPE_NAME("cortex-a76"));
    }
    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GIC);
    object_initialize_child(obj, "peripherals", &s->peripherals,
                            TYPE_BCM2712_PERIPHERALS);
}

static void bcm2712_realize(DeviceState *dev, Error **errp)
{
    BCM2712State *s = BCM2712(dev);
    DeviceState *gicdev = DEVICE(&s->gic);

    for (unsigned int n = 0; n < BCM2712_NUM_CPUS; n++) {
        object_property_set_int(OBJECT(&s->cpu[n]), "mp-affinity", n << 8,
                                &error_abort);
        object_property_set_bool(OBJECT(&s->cpu[n]), "start-powered-off",
                                 n > 0, &error_abort);

        object_property_set_int(OBJECT(&s->cpu[n]), "psci-conduit",
                                QEMU_PSCI_CONDUIT_SMC, &error_abort);

        if (!qdev_realize(DEVICE(&s->cpu[n]), NULL, errp)) {
            return;
        }
    }

    object_property_set_uint(OBJECT(&s->gic), "revision", 2, &error_abort);
    object_property_set_uint(OBJECT(&s->gic), "num-cpu", BCM2712_NUM_CPUS,
                             &error_abort);
    object_property_set_uint(OBJECT(&s->gic), "num-irq",
                             BCM2712_GIC_NUM_SPIS + BCM2712_GIC_INTERNAL,
                             &error_abort);
    object_property_set_bool(OBJECT(&s->gic),
                             "has-virtualization-extensions", true,
                             &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gic), errp)) {
        return;
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 0, BCM2712_GIC_DIST_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 1, BCM2712_GIC_CPU_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 2, BCM2712_GIC_VIFACE_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 3, BCM2712_GIC_VCPU_BASE);

    for (unsigned int n = 0; n < BCM2712_NUM_CPUS; n++) {
        DeviceState *cpudev = DEVICE(&s->cpu[n]);

        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), n,
                           qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic), n + BCM2712_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic),
                           n + 2 * BCM2712_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic),
                           n + 3 * BCM2712_NUM_CPUS,
                           qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gic),
                           n + 4 * BCM2712_NUM_CPUS,
                           qdev_get_gpio_in(gicdev,
                               PPI(n, GIC400_MAINTENANCE_IRQ)));

        qdev_connect_gpio_out(cpudev, GTIMER_PHYS,
            qdev_get_gpio_in(gicdev, PPI(n, GIC400_TIMER_NS_EL1_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_VIRT,
            qdev_get_gpio_in(gicdev, PPI(n, GIC400_TIMER_VIRT_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_HYP,
            qdev_get_gpio_in(gicdev, PPI(n, GIC400_TIMER_NS_EL2_IRQ)));
        qdev_connect_gpio_out(cpudev, GTIMER_SEC,
            qdev_get_gpio_in(gicdev, PPI(n, GIC400_TIMER_S_EL1_IRQ)));
        qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0,
            qdev_get_gpio_in(gicdev, PPI(n, VIRTUAL_PMU_IRQ)));
    }

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->peripherals), errp)) {
        return;
    }
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->peripherals.uart), 0,
                       qdev_get_gpio_in(gicdev, BCM2712_GIC_UART_IRQ));
    for (unsigned int n = 0; n < 4; n++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->peripherals.systimer), n,
                           qdev_get_gpio_in(gicdev,
                               BCM2712_GIC_SYSTIMER_IRQ + n));
    }
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->peripherals.sdhci), 0,
                       qdev_get_gpio_in(gicdev, BCM2712_GIC_SDHCI_IRQ));
}

static void bcm2712_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = bcm2712_realize;
}

static const TypeInfo bcm2712_type_info = {
    .name = TYPE_BCM2712,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(BCM2712State),
    .instance_init = bcm2712_init,
    .class_init = bcm2712_class_init,
};

static void bcm2712_register_types(void)
{
    type_register_static(&bcm2712_type_info);
}

type_init(bcm2712_register_types)
