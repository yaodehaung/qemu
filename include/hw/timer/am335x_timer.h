/*
 * TI AM335x DMTimer (ti,am335x-timer) emulation.
 *
 * Implements the register subset the mainline Linux clocksource/clockevent
 * driver (dmtimer_systimer_init(), drivers/clocksource/timer-ti-dm-systimer.c)
 * actually touches to run a free-running up-counter with an overflow
 * interrupt: TIDR, TIOCP_CFG, TISTAT (the ti-sysc reset-status register
 * the "ti,sysc-omap4-timer" wrapper polls, not part of the DMTimer's own
 * functional register set), the IRQSTATUS(_RAW)/IRQENABLE_SET/IRQENABLE_CLR
 * pair, TCLR, TCRR, TLDR and TTGR. Capture (TCAR1/2) and hardware trigger
 * output are not modelled.
 *
 * The functional clock is fixed at AM335X_TIMER_CLK_FREQ instead of being
 * derived from the (unmodelled) PRCM clock tree.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_TIMER_AM335X_TIMER_H
#define HW_TIMER_AM335X_TIMER_H

#include "hw/core/ptimer.h"
#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_AM335X_TIMER "am335x-timer"
OBJECT_DECLARE_SIMPLE_TYPE(AM335XTimerState, AM335X_TIMER)

/* Fixed functional clock frequency; real hardware sources this from PRCM. */
#define AM335X_TIMER_CLK_FREQ  24000000

struct AM335XTimerState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
    qemu_irq irq;
    ptimer_state *timer;

    uint32_t tiocp_cfg;
    uint32_t irq_status_raw;
    uint32_t irq_enable;
    uint32_t tclr;
    uint32_t tldr;
    /* Valid only while the counter is stopped; see am335x_timer_tcrr(). */
    uint32_t tcrr;
};

#endif /* HW_TIMER_AM335X_TIMER_H */
