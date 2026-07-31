/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "exec/translation-block.h"
#include "accel/tcg/cpu-ops.h"
#include "tcg/tcg.h"

static void q32_cpu_set_pc(CPUState *cs, vaddr value)
{
    Q32_CPU(cs)->env.pc = value;
}

static vaddr q32_cpu_get_pc(CPUState *cs)
{
    return Q32_CPU(cs)->env.pc;
}

static TCGTBCPUState q32_get_tb_cpu_state(CPUState *cs)
{
    CPUQ32State *env = cpu_env(cs);
    return (TCGTBCPUState){ .pc = env->pc };
}

static void q32_synchronize_from_tb(CPUState *cs, const TranslationBlock *tb)
{
    tcg_debug_assert(!tcg_cflags_has(cs, CF_PCREL));
    Q32_CPU(cs)->env.pc = tb->pc;
}

static void q32_restore_state_to_opc(CPUState *cs,
                                     const TranslationBlock *tb,
                                     const uint64_t *data)
{
    Q32_CPU(cs)->env.pc = data[0];
}

static int q32_mmu_index(CPUState *cs, bool ifetch)
{
    return 0;
}

static void q32_disas_set_info(const CPUState *cs, disassemble_info *info)
{
    info->endian = BFD_ENDIAN_LITTLE;
    info->print_insn = print_insn_q32;
}

static void q32_reset_hold(Object *obj, ResetType type)
{
    Q32CPU *cpu = Q32_CPU(obj);
    Q32CPUClass *qcc = Q32_CPU_GET_CLASS(obj);

    if (qcc->parent_phases.hold) {
        qcc->parent_phases.hold(obj, type);
    }
    memset(&cpu->env, 0, sizeof(cpu->env));
    CPU(cpu)->exception_index = -1;
}

static void q32_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    Q32CPUClass *qcc = Q32_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }
    qemu_init_vcpu(cs);
    cpu_reset(cs);
    qcc->parent_realize(dev, errp);
}

static ObjectClass *q32_class_by_name(const char *name)
{
    g_autofree char *typename = g_strdup_printf(Q32_CPU_TYPE_NAME("%s"), name);
    return object_class_by_name(typename);
}

static const TCGCPUOps q32_tcg_ops = {
    .initialize = q32_translate_init,
    .translate_code = q32_translate_code,
    .get_tb_cpu_state = q32_get_tb_cpu_state,
    .synchronize_from_tb = q32_synchronize_from_tb,
    .restore_state_to_opc = q32_restore_state_to_opc,
    .mmu_index = q32_mmu_index,
};

static void q32_class_init(ObjectClass *oc, const void *data)
{
    Q32CPUClass *qcc = Q32_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, q32_realize, &qcc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, q32_reset_hold, NULL,
                                       &qcc->parent_phases);
    cc->class_by_name = q32_class_by_name;
    cc->dump_state = q32_cpu_dump_state;
    cc->set_pc = q32_cpu_set_pc;
    cc->get_pc = q32_cpu_get_pc;
    cc->disas_set_info = q32_disas_set_info;
    cc->tcg_ops = &q32_tcg_ops;
}

static const TypeInfo q32_types[] = {
    {
        .name = TYPE_Q32_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(Q32CPU),
        .instance_align = __alignof(Q32CPU),
        .abstract = true,
        .class_size = sizeof(Q32CPUClass),
        .class_init = q32_class_init,
    },
    {
        .name = Q32_CPU_TYPE_NAME("q32-v1"),
        .parent = TYPE_Q32_CPU,
    },
};

DEFINE_TYPES(q32_types)
