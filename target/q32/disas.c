/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "qemu/osdep.h"
#include "disas/dis-asm.h"
#include "cpu.h"

typedef disassemble_info DisasContext;

static int ex_shift_2(DisasContext *ctx, int value)
{
    return value << 2;
}

static int ex_shift_12(DisasContext *ctx, int value)
{
    return value << 12;
}

#include "decode-insns.c.inc"

static const char * const regnames[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

#define PRINT_I(NAME)                                                 \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %s, %d",           \
                       regnames[a->rd], regnames[a->rs1], a->imm);    \
    return true;                                                     \
}

#define PRINT_R(NAME)                                                 \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %s, %s",           \
                       regnames[a->rd], regnames[a->rs1],             \
                       regnames[a->rs2]);                             \
    return true;                                                     \
}

#define PRINT_SHIFT(NAME)                                             \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %s, %u",           \
                       regnames[a->rd], regnames[a->rs1], a->shamt);  \
    return true;                                                     \
}

#define PRINT_LOAD(NAME)                                              \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %d(%s)",           \
                       regnames[a->rd], a->imm, regnames[a->rs1]);    \
    return true;                                                     \
}

#define PRINT_STORE(NAME)                                             \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %d(%s)",           \
                       regnames[a->rs2], a->imm, regnames[a->rs1]);   \
    return true;                                                     \
}

#define PRINT_BRANCH(NAME)                                            \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, %s, %d",           \
                       regnames[a->rs1], regnames[a->rs2], a->imm);   \
    return true;                                                     \
}

PRINT_LOAD(lb) PRINT_LOAD(lh) PRINT_LOAD(lw)
PRINT_LOAD(lbu) PRINT_LOAD(lhu)
PRINT_STORE(sb) PRINT_STORE(sh) PRINT_STORE(sw)
PRINT_I(slti) PRINT_I(sltiu) PRINT_I(xori) PRINT_I(ori) PRINT_I(andi)
PRINT_SHIFT(slli) PRINT_SHIFT(srli) PRINT_SHIFT(srai)
PRINT_R(add) PRINT_R(sub) PRINT_R(sll) PRINT_R(slt) PRINT_R(sltu)
PRINT_R(xor) PRINT_R(srl) PRINT_R(sra) PRINT_R(or) PRINT_R(and)
PRINT_BRANCH(beq) PRINT_BRANCH(bne) PRINT_BRANCH(blt)
PRINT_BRANCH(bge) PRINT_BRANCH(bltu) PRINT_BRANCH(bgeu)

static bool trans_addi(DisasContext *info, arg_addi *a)
{
    info->fprintf_func(info->stream, "addi %s, %s, %d",
                       regnames[a->rd], regnames[a->rs1], a->imm);
    return true;
}

static bool trans_jalr(DisasContext *info, arg_jalr *a)
{
    info->fprintf_func(info->stream, "jalr %s, %d(%s)",
                       regnames[a->rd], a->imm, regnames[a->rs1]);
    return true;
}

static bool trans_jal(DisasContext *info, arg_jal *a)
{
    info->fprintf_func(info->stream, "jal %s, %d", regnames[a->rd], a->imm);
    return true;
}

#define PRINT_U(NAME)                                                 \
static bool trans_##NAME(DisasContext *info, arg_##NAME *a)          \
{                                                                    \
    info->fprintf_func(info->stream, #NAME " %s, 0x%x",             \
                       regnames[a->rd], (uint32_t)a->imm >> 12);      \
    return true;                                                     \
}

PRINT_U(lui)
PRINT_U(auipc)

static bool trans_ecall(DisasContext *info, arg_ecall *a)
{
    info->fprintf_func(info->stream, "ecall");
    return true;
}

int print_insn_q32(bfd_vma addr, disassemble_info *info)
{
    bfd_byte buffer[4];
    uint32_t insn;
    int status = info->read_memory_func(addr, buffer, 4, info);

    if (status) {
        info->memory_error_func(status, addr, info);
        return -1;
    }
    insn = bfd_getl32(buffer);
    if (!decode(info, insn)) {
        info->fprintf_func(info->stream, ".word 0x%08x", insn);
    }
    return 4;
}
