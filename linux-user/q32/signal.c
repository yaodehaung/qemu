/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "qemu.h"
#include "user-internals.h"
#include "signal-common.h"
#include "linux-user/trace.h"

struct target_sigcontext {
    abi_uint pc;
    abi_uint gpr[31];
};

struct target_ucontext {
    abi_uint uc_flags;
    abi_ptr uc_link;
    target_stack_t uc_stack;
    target_sigset_t uc_sigmask;
    uint8_t reserved[120];
    uint8_t align_pad[12];
    struct target_sigcontext uc_mcontext;
};

struct target_rt_sigframe {
    struct target_siginfo info;
    struct target_ucontext uc;
};

QEMU_BUILD_BUG_ON(sizeof(struct target_sigcontext) != 128);
QEMU_BUILD_BUG_ON(offsetof(struct target_ucontext, uc_sigmask) != 0x14);
QEMU_BUILD_BUG_ON(offsetof(struct target_ucontext, uc_mcontext) != 0xa0);
QEMU_BUILD_BUG_ON(sizeof(struct target_ucontext) != 288);
QEMU_BUILD_BUG_ON(sizeof(struct target_rt_sigframe) != 416);

static abi_ulong get_sigframe(struct target_sigaction *ka, CPUQ32State *env)
{
    abi_ulong sp = get_sp_from_cpustate(env);
    size_t size = sizeof(struct target_rt_sigframe);

    if (on_sig_stack(sp) && !likely(on_sig_stack(sp - size))) {
        return -1;
    }
    return QEMU_ALIGN_DOWN(target_sigsp(sp, ka) - size, 16);
}

static void save_sigcontext(struct target_sigcontext *sc, CPUQ32State *env)
{
    int i;

    __put_user(env->pc, &sc->pc);
    for (i = 1; i < 32; i++) {
        __put_user(env->gpr[i], &sc->gpr[i - 1]);
    }
}

static void restore_sigcontext(CPUQ32State *env,
                               struct target_sigcontext *sc)
{
    int i;

    __get_user(env->pc, &sc->pc);
    for (i = 1; i < 32; i++) {
        __get_user(env->gpr[i], &sc->gpr[i - 1]);
    }
    env->gpr[0] = 0;
}

void setup_rt_frame(int sig, struct target_sigaction *ka,
                    target_siginfo_t *info, target_sigset_t *set,
                    CPUQ32State *env)
{
    abi_ulong frame_addr = get_sigframe(ka, env);
    struct target_rt_sigframe *frame;
    int i;

    trace_user_setup_rt_frame(env, frame_addr);
    if (!lock_user_struct(VERIFY_WRITE, frame, frame_addr, 0)) {
        goto badframe;
    }

    memset(frame, 0, sizeof(*frame));
    frame->info = *info;
    target_save_altstack(&frame->uc.uc_stack, env);
    for (i = 0; i < TARGET_NSIG_WORDS; i++) {
        __put_user(set->sig[i], &frame->uc.uc_sigmask.sig[i]);
    }
    save_sigcontext(&frame->uc.uc_mcontext, env);
    unlock_user_struct(frame, frame_addr, 1);

    env->gpr[1] = default_rt_sigreturn;
    env->gpr[2] = frame_addr;
    env->gpr[10] = sig;
    env->gpr[11] = frame_addr + offsetof(struct target_rt_sigframe, info);
    env->gpr[12] = frame_addr + offsetof(struct target_rt_sigframe, uc);
    env->pc = ka->_sa_handler;
    return;

badframe:
    unlock_user_struct(frame, frame_addr, 1);
    if (sig == TARGET_SIGSEGV) {
        ka->_sa_handler = TARGET_SIG_DFL;
    }
    force_sig(TARGET_SIGSEGV);
}

long do_rt_sigreturn(CPUQ32State *env)
{
    abi_ulong frame_addr = env->gpr[2];
    struct target_rt_sigframe *frame;
    target_sigset_t target_set;
    sigset_t blocked;
    int i;

    trace_user_do_sigreturn(env, frame_addr);
    if (!lock_user_struct(VERIFY_READ, frame, frame_addr, 1)) {
        goto badframe;
    }
    target_sigemptyset(&target_set);
    for (i = 0; i < TARGET_NSIG_WORDS; i++) {
        __get_user(target_set.sig[i], &frame->uc.uc_sigmask.sig[i]);
    }
    target_to_host_sigset_internal(&blocked, &target_set);
    set_sigmask(&blocked);
    restore_sigcontext(env, &frame->uc.uc_mcontext);
    target_restore_altstack(&frame->uc.uc_stack, env);
    unlock_user_struct(frame, frame_addr, 0);
    return -QEMU_ESIGRETURN;

badframe:
    unlock_user_struct(frame, frame_addr, 0);
    force_sig(TARGET_SIGSEGV);
    return 0;
}

void setup_sigtramp(abi_ulong sigtramp_page)
{
    uint32_t *tramp = lock_user(VERIFY_WRITE, sigtramp_page, 8, 0);

    assert(tramp);
    __put_user(0x08b008ab, tramp);
    __put_user(0x0000001f, tramp + 1);
    default_rt_sigreturn = sigtramp_page;
    unlock_user(tramp, sigtramp_page, 8);
}
