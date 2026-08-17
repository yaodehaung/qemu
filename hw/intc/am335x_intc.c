/*
 * TI AM335x interrupt controller (INTC) emulation.
 *
 * See include/hw/intc/am335x_intc.h for the modelling scope.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev.h"
#include "hw/intc/am335x_intc.h"
#include "migration/vmstate.h"
#include "qemu/host-utils.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define INTC_REVISION       0x00
#define INTC_SYSCONFIG      0x10
#define INTC_SYSCONFIG_SOFTRESET   (1 << 1)
#define INTC_SYSSTATUS      0x14
#define INTC_SIR_IRQ        0x40
#define INTC_SIR_FIQ        0x44
#define INTC_CONTROL        0x48
#define INTC_PROTECTION     0x4C
#define INTC_IDLE           0x50
#define INTC_IRQ_PRIORITY   0x60
#define INTC_FIQ_PRIORITY   0x64
#define INTC_THRESHOLD      0x68

#define INTC_BANK_BASE      0x80
#define INTC_BANK_STRIDE    0x20
#define INTC_BANK_END       (INTC_BANK_BASE + \
                             AM335X_INTC_NUM_BANKS * INTC_BANK_STRIDE)

#define INTC_REG_ITR          0x00
#define INTC_REG_MIR           0x04
#define INTC_REG_MIR_CLEAR     0x08
#define INTC_REG_MIR_SET       0x0C
#define INTC_REG_ISR_SET       0x10
#define INTC_REG_ISR_CLEAR     0x14
#define INTC_REG_PENDING_IRQ   0x18
#define INTC_REG_PENDING_FIQ   0x1C

static void am335x_intc_update(AM335XIntcState *s)
{
    int i;
    bool pending = false;

    for (i = 0; i < AM335X_INTC_NUM_BANKS; i++) {
        if (s->itr[i] & ~s->mir[i]) {
            pending = true;
            break;
        }
    }

    qemu_set_irq(s->parent_irq, pending);
}

static void am335x_intc_set_irq(void *opaque, int irq, int level)
{
    AM335XIntcState *s = opaque;
    uint32_t *bank = &s->itr[irq / 32];

    *bank = deposit32(*bank, irq % 32, 1, !!level);
    am335x_intc_update(s);
}

static uint64_t am335x_intc_read(void *opaque, hwaddr offset, unsigned size)
{
    AM335XIntcState *s = opaque;
    unsigned bank, reg;
    int i, bit;

    switch (offset) {
    case INTC_REVISION:
        return 0x00000040;
    case INTC_SYSCONFIG:
        return s->sysconfig;
    case INTC_SYSSTATUS:
        /* Reset is modelled as instantaneous: always "done". */
        return 0x1;
    case INTC_SIR_IRQ:
        for (i = 0; i < AM335X_INTC_NUM_BANKS; i++) {
            uint32_t pending = s->itr[i] & ~s->mir[i];

            if (pending) {
                bit = ctz32(pending);
                return i * 32 + bit;
            }
        }
        return 0;
    case INTC_SIR_FIQ:
        /* No line is ever routed to FIQ. */
        return 0;
    case INTC_CONTROL:
    case INTC_PROTECTION:
    case INTC_IDLE:
    case INTC_IRQ_PRIORITY:
    case INTC_FIQ_PRIORITY:
        return 0;
    case INTC_THRESHOLD:
        return 0xff;
    default:
        break;
    }

    if (offset >= INTC_BANK_BASE && offset < INTC_BANK_END) {
        bank = (offset - INTC_BANK_BASE) / INTC_BANK_STRIDE;
        reg = (offset - INTC_BANK_BASE) % INTC_BANK_STRIDE;

        switch (reg) {
        case INTC_REG_ITR:
            return s->itr[bank];
        case INTC_REG_MIR:
        case INTC_REG_MIR_CLEAR:
        case INTC_REG_MIR_SET:
            return s->mir[bank];
        case INTC_REG_ISR_SET:
        case INTC_REG_ISR_CLEAR:
        case INTC_REG_PENDING_FIQ:
            return 0;
        case INTC_REG_PENDING_IRQ:
            return s->itr[bank] & ~s->mir[bank];
        default:
            break;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                  __func__, offset);
    return 0;
}

static void am335x_intc_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    AM335XIntcState *s = opaque;
    unsigned bank, reg;

    switch (offset) {
    case INTC_SYSCONFIG:
        if (value & INTC_SYSCONFIG_SOFTRESET) {
            device_cold_reset(DEVICE(s));
            return;
        }
        s->sysconfig = value;
        return;
    case INTC_CONTROL:
        /*
         * NEWIRQAGR/NEWFIQAGR: SIR_IRQ is computed live from itr/mir, so
         * there is no "active interrupt" latch to re-arm here.
         */
        return;
    case INTC_PROTECTION:
    case INTC_IDLE:
    case INTC_IRQ_PRIORITY:
    case INTC_FIQ_PRIORITY:
    case INTC_THRESHOLD:
        /* Accepted (touched by the real driver's init sequence), no state. */
        return;
    case INTC_SIR_IRQ:
    case INTC_SIR_FIQ:
    case INTC_SYSSTATUS:
    case INTC_REVISION:
        /* Read-only. */
        return;
    default:
        break;
    }

    if (offset >= INTC_BANK_BASE && offset < INTC_BANK_END) {
        bank = (offset - INTC_BANK_BASE) / INTC_BANK_STRIDE;
        reg = (offset - INTC_BANK_BASE) % INTC_BANK_STRIDE;

        switch (reg) {
        case INTC_REG_MIR:
            s->mir[bank] = value;
            am335x_intc_update(s);
            return;
        case INTC_REG_MIR_CLEAR:
            s->mir[bank] &= ~value;
            am335x_intc_update(s);
            return;
        case INTC_REG_MIR_SET:
            s->mir[bank] |= value;
            am335x_intc_update(s);
            return;
        case INTC_REG_ITR:
        case INTC_REG_ISR_SET:
        case INTC_REG_ISR_CLEAR:
        case INTC_REG_PENDING_IRQ:
        case INTC_REG_PENDING_FIQ:
            /* Read-only or not modelled (software-triggered interrupts). */
            return;
        default:
            break;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset 0x%" HWADDR_PRIx "\n",
                  __func__, offset);
}

static const MemoryRegionOps am335x_intc_ops = {
    .read = am335x_intc_read,
    .write = am335x_intc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const VMStateDescription vmstate_am335x_intc = {
    .name = TYPE_AM335X_INTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(sysconfig, AM335XIntcState),
        VMSTATE_UINT32_ARRAY(itr, AM335XIntcState, AM335X_INTC_NUM_BANKS),
        VMSTATE_UINT32_ARRAY(mir, AM335XIntcState, AM335X_INTC_NUM_BANKS),
        VMSTATE_END_OF_LIST()
    },
};

static void am335x_intc_reset(DeviceState *dev)
{
    AM335XIntcState *s = AM335X_INTC(dev);
    int i;

    s->sysconfig = 0;
    for (i = 0; i < AM335X_INTC_NUM_BANKS; i++) {
        s->itr[i] = 0;
        /* Reset value: every line masked, matching real silicon. */
        s->mir[i] = 0xffffffff;
    }
}

static void am335x_intc_init(Object *obj)
{
    AM335XIntcState *s = AM335X_INTC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    qdev_init_gpio_in(DEVICE(s), am335x_intc_set_irq, AM335X_INTC_NUM_IRQS);
    sysbus_init_irq(sbd, &s->parent_irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &am335x_intc_ops, s,
                          TYPE_AM335X_INTC, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void am335x_intc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, am335x_intc_reset);
    dc->desc = "TI AM335x interrupt controller";
    dc->vmsd = &vmstate_am335x_intc;
}

static const TypeInfo am335x_intc_info = {
    .name = TYPE_AM335X_INTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(AM335XIntcState),
    .instance_init = am335x_intc_init,
    .class_init = am335x_intc_class_init,
};

static void am335x_intc_register_types(void)
{
    type_register_static(&am335x_intc_info);
}

type_init(am335x_intc_register_types)
