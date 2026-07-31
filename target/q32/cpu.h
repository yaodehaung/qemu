/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef Q32_CPU_H
#define Q32_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-common.h"

struct Q32CPUClass {
    CPUClass parent_class;
    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

enum {
    Q32_EXCP_SYSCALL = 1,
    Q32_EXCP_ILLEGAL = 2,
    Q32_EXCP_ALIGN = 3,
};

typedef struct CPUArchState {
    uint32_t gpr[32];
    uint32_t pc;
    uint32_t badaddr;
} CPUQ32State;

/* Q32 linux-user only has a single address space. */
#define MMU_USER_IDX 0

struct ArchCPU {
    CPUState parent_obj;
    CPUQ32State env;
};

void q32_translate_init(void);
void q32_translate_code(CPUState *cs, TranslationBlock *tb,
                        int *max_insns, vaddr pc, void *host_pc);
void q32_cpu_dump_state(CPUState *cs, FILE *f, int flags);
int print_insn_q32(bfd_vma addr, disassemble_info *info);

#define CPU_RESOLVING_TYPE TYPE_Q32_CPU

#endif
