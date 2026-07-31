/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "loader.h"
#include "target_elf.h"

const char *get_elf_cpu_model(uint32_t eflags)
{
    return "q32-v1";
}

void elf_core_copy_regs(target_elf_gregset_t *regs, const CPUQ32State *env)
{
    int i;

    regs->pc = tswap32(env->pc);
    for (i = 1; i < 32; i++) {
        regs->regs[i - 1] = tswap32(env->gpr[i]);
    }
}
