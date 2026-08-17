/*
 * TI AM3358 SoC emulation.
 *
 * A minimal SoC model: enough of a Cortex-A8 AM335x-family chip to boot
 * an unmodified mainline Linux kernel to a serial console. See
 * docs/tmdssk3358-support-spec.md for what is and is not modelled.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_ARM_AM3358_H
#define HW_ARM_AM3358_H

#include "hw/intc/am335x_intc.h"
#include "hw/timer/am335x_timer.h"
#include "target/arm/cpu.h"
#include "qom/object.h"

#define TYPE_AM3358 "am3358"
OBJECT_DECLARE_SIMPLE_TYPE(AM3358State, AM3358)

struct AM3358State {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    ARMCPU cpu;
    AM335XIntcState intc;
    AM335XTimerState timer2;
};

/*
 * SDRAM base address. Matches the real TRM. The rest of the AM3358 memory
 * map is an implementation detail private to am3358.c; this is the one
 * address tmdssk3358.c needs to place guest RAM.
 */
#define AM3358_SDRAM_ADDR       0x80000000

#endif /* HW_ARM_AM3358_H */
