/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef Q32_TARGET_CPU_H
#define Q32_TARGET_CPU_H

#define TARGET_VALIDATE_CLONE_FLAGS 1
static inline bool target_validate_clone_flags(unsigned flags)
{
    return !(flags & (CLONE_VM | CLONE_THREAD | CLONE_SETTLS));
}

static inline void cpu_clone_regs_child(CPUQ32State *env,
                                        target_ulong newsp, unsigned flags)
{
    if (newsp) {
        env->gpr[2] = newsp;
    }
    env->gpr[10] = 0;
}

static inline void cpu_clone_regs_parent(CPUQ32State *env, unsigned flags) { }
static inline void cpu_set_tls(CPUQ32State *env, target_ulong newtls)
{
    env->gpr[4] = newtls;
}
static inline abi_ulong get_sp_from_cpustate(CPUQ32State *env)
{
    return env->gpr[2];
}
#endif
