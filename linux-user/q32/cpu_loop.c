/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "user-internals.h"
#include "user/cpu_loop.h"
#include "signal-common.h"

void cpu_loop(CPUQ32State *env)
{
    CPUState *cs = env_cpu(env);

    for (;;) {
        abi_long ret;
        int trapnr;

        cpu_exec_start(cs);
        trapnr = cpu_exec(cs);
        cpu_exec_end(cs);
        qemu_process_cpu_events(cs);

        switch (trapnr) {
        case Q32_EXCP_SYSCALL:
            env->pc += 4;
            ret = do_syscall(env, env->gpr[17], env->gpr[10], env->gpr[11],
                             env->gpr[12], env->gpr[13], env->gpr[14],
                             env->gpr[15], 0, 0);
            if (ret == -QEMU_ERESTARTSYS) {
                env->pc -= 4;
            } else if (ret != -QEMU_ESIGRETURN && ret != -QEMU_ESETPC) {
                env->gpr[10] = ret;
            }
            break;
        case Q32_EXCP_ILLEGAL:
            force_sig_fault(TARGET_SIGILL, TARGET_ILL_ILLOPC, env->pc);
            break;
        case Q32_EXCP_ALIGN:
            force_sig_fault(TARGET_SIGBUS, TARGET_BUS_ADRALN, env->badaddr);
            break;
        case EXCP_INTERRUPT:
            break;
        case EXCP_DEBUG:
            force_sig_fault(TARGET_SIGTRAP, TARGET_TRAP_BRKPT, env->pc);
            break;
        default:
            g_assert_not_reached();
        }
        env->gpr[0] = 0;
        process_pending_signals(env);
    }
}

void init_main_thread(CPUState *cs, struct image_info *info)
{
    CPUQ32State *env = cpu_env(cs);
    env->pc = info->entry;
    env->gpr[2] = info->start_stack;
}
