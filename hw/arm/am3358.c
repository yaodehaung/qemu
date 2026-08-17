/*
 * TI AM3358 SoC emulation.
 *
 * See include/hw/arm/am3358.h for the modelling scope.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/arm/am3358.h"
#include "hw/char/serial-mm.h"
#include "hw/misc/unimp.h"
#include "hw/core/qdev-properties.h"
#include "system/address-spaces.h"
#include "system/system.h"
#include "target/arm/cpu-qom.h"

/*
 * AM3358 memory map (subset). Addresses match the real TRM even though
 * only a handful of modules are implemented, so that the layout stays
 * credible and easy to extend. Private to this file: tmdssk3358.c only
 * needs AM3358_SDRAM_ADDR, which stays public in am3358.h.
 */
enum {
    AM3358_DEV_UART0,
    AM3358_DEV_PRCM,
    AM3358_DEV_SCM,
    AM3358_DEV_INTC,
    AM3358_DEV_TIMER2,
};

static const MemMapEntry am3358_memmap[] = {
    [AM3358_DEV_UART0]  = { 0x44E09000, 0 },
    [AM3358_DEV_PRCM]   = { 0x44E00000, 0x1000 },
    [AM3358_DEV_SCM]    = { 0x44E10000, 0x2000 },
    [AM3358_DEV_INTC]   = { 0x48200000, 0 },
    [AM3358_DEV_TIMER2] = { 0x48040000, 0 },
};

#define AM3358_UART0_IRQ        72
#define AM3358_TIMER2_IRQ       68

static void am3358_init(Object *obj)
{
    AM3358State *s = AM3358(obj);

    object_initialize_child(obj, "cpu", &s->cpu,
                            ARM_CPU_TYPE_NAME("cortex-a8"));
    object_initialize_child(obj, "intc", &s->intc, TYPE_AM335X_INTC);
    object_initialize_child(obj, "timer2", &s->timer2, TYPE_AM335X_TIMER);
}

static void am3358_realize(DeviceState *dev, Error **errp)
{
    AM3358State *s = AM3358(dev);

    if (!qdev_realize(DEVICE(&s->cpu), NULL, errp)) {
        return;
    }

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->intc), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->intc), 0,
                    am3358_memmap[AM3358_DEV_INTC].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->intc), 0,
                       qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->timer2), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->timer2), 0,
                    am3358_memmap[AM3358_DEV_TIMER2].base);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->timer2), 0,
                       qdev_get_gpio_in(DEVICE(&s->intc), AM3358_TIMER2_IRQ));

    /*
     * ns16550a, not the TI-specific ti,am3358-uart: this keeps the guest
     * on the plain 8250 driver, which never touches the OMAP-specific
     * UART_OMAP_MDR1 mode register that hw/char/serial.c doesn't model.
     */
    serial_mm_init(get_system_memory(), am3358_memmap[AM3358_DEV_UART0].base, 2,
                   qdev_get_gpio_in(DEVICE(&s->intc), AM3358_UART0_IRQ),
                   115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);

    /*
     * PRCM and Control Module are not modelled: every read returns 0,
     * which the Linux am33xx clock driver's IDLEST polling reads as
     * "module already functional", and pinctrl-single's writes are
     * harmlessly discarded. See docs/tmdssk3358-support-spec.md.
     */
    create_unimplemented_device("am3358.prcm", am3358_memmap[AM3358_DEV_PRCM].base,
                                am3358_memmap[AM3358_DEV_PRCM].size);
    create_unimplemented_device("am3358.scm", am3358_memmap[AM3358_DEV_SCM].base,
                                am3358_memmap[AM3358_DEV_SCM].size);
}

static void am3358_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = am3358_realize;
    dc->desc = "TI AM3358 SoC";
    /* Reason: uses serial_hd() in realize, like other minimal SoC models. */
    dc->user_creatable = false;
}

static const TypeInfo am3358_type_info = {
    .name = TYPE_AM3358,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(AM3358State),
    .instance_init = am3358_init,
    .class_init = am3358_class_init,
};

static void am3358_register_types(void)
{
    type_register_static(&am3358_type_info);
}

type_init(am3358_register_types)
