/*
 * TI AM335x DMTimer (ti,am335x-timer) emulation.
 *
 * See include/hw/timer/am335x_timer.h for the modelling scope.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev.h"
#include "hw/timer/am335x_timer.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define TIDR            0x00
#define TIOCP_CFG       0x10
#define TIOCP_CFG_SOFTRESET    (1 << 0)
/*
 * Not in the DMTimer's own functional register set: ti-sysc (the Linux
 * interconnect-target-module driver every "ti,sysc"-wrapped device
 * front-ends, see drivers/bus/ti-sysc.c) expects a reset-status register
 * here for the "ti,sysc-omap4-timer" binding. Reset is modelled as
 * instantaneous, so it always reads back "done".
 */
#define TISTAT          0x14
#define IRQSTATUS_RAW   0x24
#define IRQSTATUS       0x28
#define IRQENABLE_SET   0x2C
#define IRQENABLE_CLR   0x30
#define IRQWAKEEN       0x34
#define TCLR            0x38
#define TCLR_ST                (1 << 0)
#define TCLR_AR                (1 << 1)
#define TCLR_PTV_SHIFT         2
#define TCLR_PTV_LEN           3
#define TCLR_PRE               (1 << 5)
#define TCRR            0x3C
#define TLDR            0x40
#define TTGR            0x44
#define TWPS            0x48
#define TMAR            0x4C

#define IRQ_MAT_BIT     (1 << 0)
#define IRQ_OVF_BIT     (1 << 1)
#define IRQ_TCAR_BIT    (1 << 2)

/* Ticks remaining until the free-running counter wraps past 0xffffffff. */
static uint64_t am335x_timer_ticks_to_overflow(uint32_t tcrr)
{
    return 0x100000000ULL - tcrr;
}

/* Must be called inside a ptimer transaction block. */
static void am335x_timer_reload(AM335XTimerState *s, uint32_t tcrr)
{
    uint32_t prescaler = 1;

    if (s->tclr & TCLR_PRE) {
        prescaler <<= extract32(s->tclr, TCLR_PTV_SHIFT, TCLR_PTV_LEN) + 1;
    }
    ptimer_set_freq(s->timer, AM335X_TIMER_CLK_FREQ / prescaler);

    if (s->tclr & TCLR_ST) {
        ptimer_set_limit(s->timer, am335x_timer_ticks_to_overflow(tcrr), 1);
        ptimer_run(s->timer, 0);
    } else {
        ptimer_stop(s->timer);
        s->tcrr = tcrr;
    }
}

/* Current 32-bit counter value, whether running or stopped. */
static uint32_t am335x_timer_tcrr(AM335XTimerState *s)
{
    if (!(s->tclr & TCLR_ST)) {
        return s->tcrr;
    }
    return (uint32_t)(0x100000000ULL - ptimer_get_count(s->timer));
}

static void am335x_timer_update_irq(AM335XTimerState *s)
{
    qemu_set_irq(s->irq, !!(s->irq_status_raw & s->irq_enable));
}

static void am335x_timer_tick(void *opaque)
{
    AM335XTimerState *s = opaque;

    s->irq_status_raw |= IRQ_OVF_BIT;

    /*
     * Called by the ptimer core from inside ptimer_reload(), which already
     * wraps this callback in its own transaction (see the comment above
     * ptimer_reload() in hw/core/ptimer.c) — a nested begin/commit here
     * trips ptimer_transaction_begin()'s "!s->in_transaction" assert.
     * AR reloads from TLDR; otherwise the counter free-wraps through 0.
     */
    ptimer_set_limit(s->timer,
                      am335x_timer_ticks_to_overflow(
                          (s->tclr & TCLR_AR) ? s->tldr : 0), 1);

    am335x_timer_update_irq(s);
}

static uint64_t am335x_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    AM335XTimerState *s = opaque;

    switch (offset) {
    case TIDR:
        return 0x40000000;
    case TIOCP_CFG:
        return s->tiocp_cfg;
    case TISTAT:
        return 0x1;
    case IRQSTATUS_RAW:
        return s->irq_status_raw;
    case IRQSTATUS:
        return s->irq_status_raw & s->irq_enable;
    case IRQENABLE_SET:
    case IRQENABLE_CLR:
        return s->irq_enable;
    case IRQWAKEEN:
        return 0;
    case TCLR:
        return s->tclr;
    case TCRR:
        return am335x_timer_tcrr(s);
    case TLDR:
        return s->tldr;
    case TTGR:
        return 0;
    case TWPS:
        /* Writes take effect synchronously: nothing is ever posted. */
        return 0;
    case TMAR:
        return 0;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
        return 0;
    }
}

static void am335x_timer_write(void *opaque, hwaddr offset, uint64_t value,
                               unsigned size)
{
    AM335XTimerState *s = opaque;

    switch (offset) {
    case TIOCP_CFG:
        if (value & TIOCP_CFG_SOFTRESET) {
            device_cold_reset(DEVICE(s));
            return;
        }
        s->tiocp_cfg = value;
        return;
    case TISTAT:
        /* Read-only. */
        return;
    case IRQSTATUS_RAW:
        s->irq_status_raw |= value;
        am335x_timer_update_irq(s);
        return;
    case IRQSTATUS:
        s->irq_status_raw &= ~value;
        am335x_timer_update_irq(s);
        return;
    case IRQENABLE_SET:
        s->irq_enable |= value;
        am335x_timer_update_irq(s);
        return;
    case IRQENABLE_CLR:
        s->irq_enable &= ~value;
        am335x_timer_update_irq(s);
        return;
    case IRQWAKEEN:
        return;
    case TCLR: {
        /* Capture the live count under the *old* TCLR before switching. */
        uint32_t tcrr = am335x_timer_tcrr(s);

        s->tclr = value;
        ptimer_transaction_begin(s->timer);
        am335x_timer_reload(s, tcrr);
        ptimer_transaction_commit(s->timer);
        return;
    }
    case TCRR:
        ptimer_transaction_begin(s->timer);
        am335x_timer_reload(s, value);
        ptimer_transaction_commit(s->timer);
        return;
    case TLDR:
        s->tldr = value;
        return;
    case TTGR:
        /* Any write immediately reloads the counter from TLDR. */
        ptimer_transaction_begin(s->timer);
        am335x_timer_reload(s, s->tldr);
        ptimer_transaction_commit(s->timer);
        return;
    case TWPS:
        return;
    case TMAR:
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, offset);
        return;
    }
}

static const MemoryRegionOps am335x_timer_ops = {
    .read = am335x_timer_read,
    .write = am335x_timer_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const VMStateDescription vmstate_am335x_timer = {
    .name = TYPE_AM335X_TIMER,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(tiocp_cfg, AM335XTimerState),
        VMSTATE_UINT32(irq_status_raw, AM335XTimerState),
        VMSTATE_UINT32(irq_enable, AM335XTimerState),
        VMSTATE_UINT32(tclr, AM335XTimerState),
        VMSTATE_UINT32(tldr, AM335XTimerState),
        VMSTATE_UINT32(tcrr, AM335XTimerState),
        VMSTATE_PTIMER(timer, AM335XTimerState),
        VMSTATE_END_OF_LIST()
    },
};

static void am335x_timer_reset(DeviceState *dev)
{
    AM335XTimerState *s = AM335X_TIMER(dev);

    s->tiocp_cfg = 0;
    s->irq_status_raw = 0;
    s->irq_enable = 0;
    s->tclr = 0;
    s->tldr = 0;
    s->tcrr = 0;

    ptimer_transaction_begin(s->timer);
    ptimer_stop(s->timer);
    ptimer_set_freq(s->timer, AM335X_TIMER_CLK_FREQ);
    ptimer_transaction_commit(s->timer);

    am335x_timer_update_irq(s);
}

static void am335x_timer_init(Object *obj)
{
    AM335XTimerState *s = AM335X_TIMER(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &am335x_timer_ops, s,
                          TYPE_AM335X_TIMER, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);

    s->timer = ptimer_init(am335x_timer_tick, s,
                           PTIMER_POLICY_NO_COUNTER_ROUND_DOWN |
                           PTIMER_POLICY_CONTINUOUS_TRIGGER |
                           PTIMER_POLICY_WRAP_AFTER_ONE_PERIOD);
}

static void am335x_timer_finalize(Object *obj)
{
    AM335XTimerState *s = AM335X_TIMER(obj);

    ptimer_free(s->timer);
}

static void am335x_timer_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, am335x_timer_reset);
    dc->desc = "TI AM335x DMTimer";
    dc->vmsd = &vmstate_am335x_timer;
}

static const TypeInfo am335x_timer_info = {
    .name = TYPE_AM335X_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AM335XTimerState),
    .instance_init = am335x_timer_init,
    .instance_finalize = am335x_timer_finalize,
    .class_init = am335x_timer_class_init,
};

static void am335x_timer_register_types(void)
{
    type_register_static(&am335x_timer_info);
}

type_init(am335x_timer_register_types)
