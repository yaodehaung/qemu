/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "accel/tcg/cpu-loop.h"
#include "cpu.h"
#include "exec/helper-proto.h"

G_NORETURN void HELPER(raise_exception)(CPUQ32State *env, uint32_t excp)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = excp;
    cpu_loop_exit(cs);
}

G_NORETURN void HELPER(raise_align)(CPUQ32State *env, uint32_t addr)
{
    env->badaddr = addr;
    HELPER(raise_exception)(env, Q32_EXCP_ALIGN);
}
