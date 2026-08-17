/*
 * TI AM335x interrupt controller (INTC) emulation.
 *
 * Implements the register subset the mainline Linux "ti,am33xx-intc"
 * driver (drivers/irqchip/irq-omap-intc.c) actually touches: enough to
 * reset, mask/unmask individual lines, and read back the highest
 * priority pending unmasked interrupt. Priority (ILR) and FIQ routing
 * are not modelled — every line is treated as IRQ-routed with equal
 * priority, which matches what a minimal console-boot guest observes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INTC_AM335X_INTC_H
#define HW_INTC_AM335X_INTC_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_AM335X_INTC "am335x-intc"
OBJECT_DECLARE_SIMPLE_TYPE(AM335XIntcState, AM335X_INTC)

#define AM335X_INTC_NUM_IRQS   96
#define AM335X_INTC_NUM_BANKS  DIV_ROUND_UP(AM335X_INTC_NUM_IRQS, 32)

struct AM335XIntcState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
    qemu_irq parent_irq;

    uint32_t sysconfig;
    /* Raw (pre-mask) level of each input line, one bit per IRQ. */
    uint32_t itr[AM335X_INTC_NUM_BANKS];
    /* Mask register: 1 = masked/disabled, matches real MIR reset value. */
    uint32_t mir[AM335X_INTC_NUM_BANKS];
};

#endif /* HW_INTC_AM335X_INTC_H */
