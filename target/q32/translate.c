/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "cpu.h"
#include "tcg/tcg-op.h"
#include "exec/translator.h"
#include "exec/translation-block.h"
#include "exec/target_page.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"

#define HELPER_H "helper.h"
#include "exec/helper-info.c.inc"
#undef HELPER_H

typedef struct DisasContext {
    DisasContextBase base;
} DisasContext;

static int ex_shift_2(DisasContext *ctx, int value)
{
    return value << 2;
}

static int ex_shift_12(DisasContext *ctx, int value)
{
    return value << 12;
}

#include "decode-insns.c.inc"

static TCGv_i32 cpu_gpr[32];
static TCGv_i32 cpu_pc;
static TCGv_i32 cpu_badaddr;

static TCGv_i32 get_gpr(unsigned reg)
{
    return reg ? cpu_gpr[reg] : tcg_constant_i32(0);
}

static void set_gpr(unsigned reg, TCGv_i32 value)
{
    if (reg) {
        tcg_gen_mov_i32(cpu_gpr[reg], value);
    }
}

static void gen_exception(DisasContext *ctx, unsigned excp)
{
    tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
    gen_helper_raise_exception(tcg_env, tcg_constant_i32(excp));
    ctx->base.is_jmp = DISAS_NORETURN;
}

static bool trans_addi(DisasContext *ctx, arg_addi *a)
{
    TCGv_i32 value = tcg_temp_new_i32();
    tcg_gen_addi_i32(value, get_gpr(a->rs1), a->imm);
    set_gpr(a->rd, value);
    return true;
}

#define GEN_IMM_LOGIC(NAME, OP)                                      \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_##OP##_i32(value, get_gpr(a->rs1),                       \
                       tcg_constant_i32(a->imm));                    \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_IMM_LOGIC(xori, xor)
GEN_IMM_LOGIC(ori, or)
GEN_IMM_LOGIC(andi, and)

#define GEN_IMM_CMP(NAME, COND)                                      \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_setcondi_i32(COND, value, get_gpr(a->rs1), a->imm);      \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_IMM_CMP(slti, TCG_COND_LT)
GEN_IMM_CMP(sltiu, TCG_COND_LTU)

#define GEN_SHIFT_IMM(NAME, OP)                                      \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_##OP##i_i32(value, get_gpr(a->rs1), a->shamt);           \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_SHIFT_IMM(slli, shl)
GEN_SHIFT_IMM(srli, shr)
GEN_SHIFT_IMM(srai, sar)

#define GEN_REG_BIN(NAME, OP)                                        \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_##OP##_i32(value, get_gpr(a->rs1), get_gpr(a->rs2));     \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_REG_BIN(add, add)
GEN_REG_BIN(sub, sub)
GEN_REG_BIN(xor, xor)
GEN_REG_BIN(or, or)
GEN_REG_BIN(and, and)

#define GEN_REG_CMP(NAME, COND)                                      \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_setcond_i32(COND, value, get_gpr(a->rs1),                \
                        get_gpr(a->rs2));                            \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_REG_CMP(slt, TCG_COND_LT)
GEN_REG_CMP(sltu, TCG_COND_LTU)

#define GEN_REG_SHIFT(NAME, OP)                                      \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    TCGv_i32 shamt = tcg_temp_new_i32();                             \
    TCGv_i32 value = tcg_temp_new_i32();                             \
    tcg_gen_andi_i32(shamt, get_gpr(a->rs2), 31);                    \
    tcg_gen_##OP##_i32(value, get_gpr(a->rs1), shamt);               \
    set_gpr(a->rd, value);                                           \
    return true;                                                     \
}

GEN_REG_SHIFT(sll, shl)
GEN_REG_SHIFT(srl, shr)
GEN_REG_SHIFT(sra, sar)

static bool trans_lui(DisasContext *ctx, arg_lui *a)
{
    set_gpr(a->rd, tcg_constant_i32(a->imm));
    return true;
}

static bool trans_auipc(DisasContext *ctx, arg_auipc *a)
{
    set_gpr(a->rd, tcg_constant_i32(ctx->base.pc_next + a->imm));
    return true;
}

static void gen_goto(DisasContext *ctx, TCGv_i32 dest)
{
    tcg_gen_mov_i32(cpu_pc, dest);
    tcg_gen_lookup_and_goto_ptr();
    ctx->base.is_jmp = DISAS_NORETURN;
}

static bool trans_jal(DisasContext *ctx, arg_jal *a)
{
    uint32_t pc = ctx->base.pc_next;
    set_gpr(a->rd, tcg_constant_i32(pc + 4));
    gen_goto(ctx, tcg_constant_i32(pc + a->imm));
    return true;
}

static bool trans_jalr(DisasContext *ctx, arg_jalr *a)
{
    uint32_t pc = ctx->base.pc_next;
    TCGv_i32 dest = tcg_temp_new_i32();
    tcg_gen_addi_i32(dest, get_gpr(a->rs1), a->imm);
    tcg_gen_andi_i32(dest, dest, -4);
    set_gpr(a->rd, tcg_constant_i32(pc + 4));
    gen_goto(ctx, dest);
    return true;
}

#define GEN_BRANCH(NAME, COND)                                       \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    uint32_t pc = ctx->base.pc_next;                                 \
    TCGv_i32 dest = tcg_temp_new_i32();                              \
    tcg_gen_movcond_i32(COND, dest, get_gpr(a->rs1),                 \
                        get_gpr(a->rs2), tcg_constant_i32(pc + a->imm), \
                        tcg_constant_i32(pc + 4));                    \
    gen_goto(ctx, dest);                                             \
    return true;                                                     \
}

GEN_BRANCH(beq, TCG_COND_EQ)
GEN_BRANCH(bne, TCG_COND_NE)
GEN_BRANCH(blt, TCG_COND_LT)
GEN_BRANCH(bge, TCG_COND_GE)
GEN_BRANCH(bltu, TCG_COND_LTU)
GEN_BRANCH(bgeu, TCG_COND_GEU)

static TCGv_i32 gen_addr(DisasContext *ctx, unsigned rs1, int32_t imm,
                         unsigned align)
{
    TCGv_i32 addr = tcg_temp_new_i32();
    tcg_gen_addi_i32(addr, get_gpr(rs1), imm);
    if (align > 1) {
        TCGLabel *ok = gen_new_label();
        TCGv_i32 low = tcg_temp_new_i32();
        tcg_gen_andi_i32(low, addr, align - 1);
        tcg_gen_brcondi_i32(TCG_COND_EQ, low, 0, ok);
        tcg_gen_movi_i32(cpu_pc, ctx->base.pc_next);
        gen_helper_raise_align(tcg_env, addr);
        gen_set_label(ok);
    }
    return addr;
}

static bool gen_load(DisasContext *ctx, unsigned rd, unsigned rs1,
                     int32_t imm, MemOp mop, unsigned align)
{
    TCGv_i32 addr = gen_addr(ctx, rs1, imm, align);
    TCGv_i32 value = tcg_temp_new_i32();
    tcg_gen_qemu_ld_i32(value, addr, 0, MO_LE | mop);
    set_gpr(rd, value);
    return true;
}

static bool gen_store(DisasContext *ctx, unsigned rs1, unsigned rs2,
                      int32_t imm, MemOp mop, unsigned align)
{
    TCGv_i32 addr = gen_addr(ctx, rs1, imm, align);
    tcg_gen_qemu_st_i32(get_gpr(rs2), addr, 0, MO_LE | mop);
    return true;
}

#define GEN_LOAD(NAME, MOP, ALIGN)                                   \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    return gen_load(ctx, a->rd, a->rs1, a->imm, MOP, ALIGN);         \
}

#define GEN_STORE(NAME, MOP, ALIGN)                                  \
static bool trans_##NAME(DisasContext *ctx, arg_##NAME *a)           \
{                                                                    \
    return gen_store(ctx, a->rs1, a->rs2, a->imm, MOP, ALIGN);       \
}

GEN_LOAD(lb, MO_SB, 1)
GEN_LOAD(lh, MO_SW, 2)
GEN_LOAD(lw, MO_UL, 4)
GEN_LOAD(lbu, MO_UB, 1)
GEN_LOAD(lhu, MO_UW, 2)
GEN_STORE(sb, MO_UB, 1)
GEN_STORE(sh, MO_UW, 2)
GEN_STORE(sw, MO_UL, 4)

static bool trans_ecall(DisasContext *ctx, arg_ecall *a)
{
    gen_exception(ctx, Q32_EXCP_SYSCALL);
    return true;
}

static void q32_init_disas_context(DisasContextBase *db, CPUState *cs)
{
    int bound = -(db->pc_first | TARGET_PAGE_MASK) / 4;
    db->max_insns = MIN(db->max_insns, bound);
}

static void q32_insn_start(DisasContextBase *db, CPUState *cs)
{
    tcg_gen_insn_start(db->pc_next, 0, 0);
}

static void q32_tb_start(DisasContextBase *db, CPUState *cs)
{
}

static void q32_translate_insn(DisasContextBase *db, CPUState *cs)
{
    DisasContext *ctx = container_of(db, DisasContext, base);
    uint32_t insn;

    if (db->pc_next & 3) {
        tcg_gen_movi_i32(cpu_badaddr, db->pc_next);
        gen_exception(ctx, Q32_EXCP_ALIGN);
        return;
    }
    insn = translator_ldl_end(cpu_env(cs), db, db->pc_next, MO_LE);

    if (!decode(ctx, insn)) {
        gen_exception(ctx, Q32_EXCP_ILLEGAL);
    }
    db->pc_next += 4;
}

static void q32_tb_stop(DisasContextBase *db, CPUState *cs)
{
    if (db->is_jmp == DISAS_NORETURN) {
        return;
    }
    tcg_gen_movi_i32(cpu_pc, db->pc_next);
    tcg_gen_lookup_and_goto_ptr();
}

static const TranslatorOps q32_tr_ops = {
    .init_disas_context = q32_init_disas_context,
    .tb_start = q32_tb_start,
    .insn_start = q32_insn_start,
    .translate_insn = q32_translate_insn,
    .tb_stop = q32_tb_stop,
};

void q32_translate_code(CPUState *cs, TranslationBlock *tb,
                        int *max_insns, vaddr pc, void *host_pc)
{
    DisasContext ctx;
    translator_loop(cs, tb, max_insns, pc, host_pc, &q32_tr_ops,
                    &ctx.base, TCG_TYPE_VA);
}

void q32_translate_init(void)
{
    int i;
    char *name;

    cpu_gpr[0] = NULL;
    for (i = 1; i < 32; i++) {
        name = g_strdup_printf("r%d", i);
        cpu_gpr[i] = tcg_global_mem_new_i32(tcg_env,
            offsetof(CPUQ32State, gpr[i]), name);
        g_free(name);
    }
    cpu_pc = tcg_global_mem_new_i32(tcg_env, offsetof(CPUQ32State, pc), "pc");
    cpu_badaddr = tcg_global_mem_new_i32(tcg_env,
        offsetof(CPUQ32State, badaddr), "badaddr");
}

void q32_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    CPUQ32State *env = cpu_env(cs);
    int i;

    fprintf(f, "PC=%08x BADADDR=%08x\n", env->pc, env->badaddr);
    for (i = 0; i < 32; i++) {
        fprintf(f, "R%02d=%08x%c", i, i ? env->gpr[i] : 0,
                (i & 3) == 3 ? '\n' : ' ');
    }
}
