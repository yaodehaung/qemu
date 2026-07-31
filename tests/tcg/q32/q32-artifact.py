#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build deterministic static ELF32 artifacts for Q32 TCG tests."""

import argparse
import pathlib
import re
import struct
import sys


REGISTERS = {
    "zero": 0, "ra": 1, "sp": 2, "gp": 3, "tp": 4,
    "t0": 5, "t1": 6, "t2": 7, "s0": 8, "fp": 8, "s1": 9,
    **{f"a{i}": 10 + i for i in range(8)},
    **{f"s{i}": 16 + i for i in range(2, 12)},
    **{f"t{i}": 25 + i for i in range(3, 7)},
    **{f"r{i}": i for i in range(32)},
}

OPCODES = {
    "load": 0x0B, "store": 0x1B, "opimm": 0x2B, "op": 0x3B,
    "branch": 0x4B, "jalr": 0x5B, "jal": 0x6B, "lui": 0x7B,
    "auipc": 0x0F,
}

R_OPS = {
    "add": (0, 0x00), "sub": (0, 0x20), "sll": (1, 0x00),
    "slt": (2, 0x00), "sltu": (3, 0x00), "xor": (4, 0x00),
    "srl": (5, 0x00), "sra": (5, 0x20), "or": (6, 0x00),
    "and": (7, 0x00),
}
I_OPS = {"addi": 0, "slti": 2, "sltiu": 3, "xori": 4,
         "ori": 6, "andi": 7}
SHIFT_OPS = {"slli": (1, 0x00), "srli": (5, 0x00), "srai": (5, 0x20)}
LOAD_OPS = {"lb": 0, "lh": 1, "lw": 2, "lbu": 4, "lhu": 5}
STORE_OPS = {"sb": 0, "sh": 1, "sw": 2}
BRANCH_OPS = {"beq": 0, "bne": 1, "blt": 4, "bge": 5,
              "bltu": 6, "bgeu": 7}


class SourceError(ValueError):
    pass


def integer(text):
    return int(text.strip(), 0)


def register(text):
    try:
        return REGISTERS[text.strip().lower()]
    except KeyError as exc:
        raise SourceError(f"unknown register {text!r}") from exc


def checked(value, low, high, description):
    if not low <= value <= high:
        raise SourceError(f"{description} out of range ({low}..{high})")
    return value


def split_args(text):
    return [part.strip() for part in text.split(",")]


def memory_operand(text):
    match = re.fullmatch(r"(.+)\(([^()]+)\)", text.strip())
    if not match:
        raise SourceError(f"invalid memory operand {text!r}")
    return integer(match.group(1)), register(match.group(2))


def encode_b(offset, rs1, rs2, funct3):
    if offset & 3:
        raise SourceError("branch target must be 4-byte aligned")
    q = checked(offset // 4, -2048, 2047, "branch displacement") & 0xFFF
    return (((q >> 11) & 1) << 31 | ((q >> 4) & 0x3F) << 25 |
            rs2 << 20 | rs1 << 15 | funct3 << 12 |
            ((q >> 0) & 0xF) << 8 | ((q >> 10) & 1) << 7 |
            OPCODES["branch"])


def encode_j(offset, rd):
    if offset & 3:
        raise SourceError("jump target must be 4-byte aligned")
    q = checked(offset // 4, -(1 << 19), (1 << 19) - 1,
                "jump displacement") & 0xFFFFF
    return (((q >> 19) & 1) << 31 | ((q >> 11) & 0xFF) << 12 |
            ((q >> 10) & 1) << 20 | (q & 0x3FF) << 21 |
            rd << 7 | OPCODES["jal"])


def resolve_target(text, labels):
    return labels[text] if text in labels else integer(text)


def encode_instruction(text, pc, labels):
    if text == "nop":
        return 0x0000002B
    if text == "ecall":
        return 0x0000001F
    mnemonic, operands = text.split(None, 1)
    args = split_args(operands)

    if mnemonic in R_OPS:
        rd, rs1, rs2 = map(register, args)
        funct3, funct7 = R_OPS[mnemonic]
        return (funct7 << 25 | rs2 << 20 | rs1 << 15 | funct3 << 12 |
                rd << 7 | OPCODES["op"])
    if mnemonic in I_OPS:
        rd, rs1 = register(args[0]), register(args[1])
        imm = checked(integer(args[2]), -2048, 2047, "immediate") & 0xFFF
        return (imm << 20 | rs1 << 15 | I_OPS[mnemonic] << 12 |
                rd << 7 | OPCODES["opimm"])
    if mnemonic in SHIFT_OPS:
        rd, rs1 = register(args[0]), register(args[1])
        shamt = checked(integer(args[2]), 0, 31, "shift amount")
        funct3, funct7 = SHIFT_OPS[mnemonic]
        return (funct7 << 25 | shamt << 20 | rs1 << 15 | funct3 << 12 |
                rd << 7 | OPCODES["opimm"])
    if mnemonic in LOAD_OPS:
        rd = register(args[0])
        imm, rs1 = memory_operand(args[1])
        imm = checked(imm, -2048, 2047, "load offset") & 0xFFF
        return (imm << 20 | rs1 << 15 | LOAD_OPS[mnemonic] << 12 |
                rd << 7 | OPCODES["load"])
    if mnemonic in STORE_OPS:
        rs2 = register(args[0])
        imm, rs1 = memory_operand(args[1])
        imm = checked(imm, -2048, 2047, "store offset") & 0xFFF
        return ((imm >> 5) << 25 | rs2 << 20 | rs1 << 15 |
                STORE_OPS[mnemonic] << 12 | (imm & 0x1F) << 7 |
                OPCODES["store"])
    if mnemonic in BRANCH_OPS:
        rs1, rs2 = register(args[0]), register(args[1])
        return encode_b(resolve_target(args[2], labels) - pc, rs1, rs2,
                        BRANCH_OPS[mnemonic])
    if mnemonic == "jal":
        rd = register(args[0])
        return encode_j(resolve_target(args[1], labels) - pc, rd)
    if mnemonic == "jalr":
        rd = register(args[0])
        imm, rs1 = memory_operand(args[1])
        imm = checked(imm, -2048, 2047, "JALR offset") & 0xFFF
        return imm << 20 | rs1 << 15 | rd << 7 | OPCODES["jalr"]
    if mnemonic in ("lui", "auipc"):
        rd = register(args[0])
        imm = checked(integer(args[1]), 0, 0xFFFFF, "U immediate")
        return imm << 12 | rd << 7 | OPCODES[mnemonic]
    raise SourceError(f"unknown instruction {mnemonic!r}")


def parse_source(source):
    sections = {}
    entries = []
    labels = {}
    expectations = []
    mutations = []
    word_fixups = []
    section = None
    addresses = {}

    for lineno, raw in enumerate(source.splitlines(), 1):
        line = raw.split("#", 1)[0].strip().lower()
        if not line:
            continue
        if line.startswith((".text", ".data")):
            kind, *arg = line.split()
            section = kind[1:]
            default_base = 0x10000 if section == "text" else 0x20000
            base = integer(arg[0]) if arg else default_base
            default_flags = "rx" if section == "text" else "rw"
            flag_text = arg[1].lower() if len(arg) > 1 else default_flags
            if not flag_text or set(flag_text) - set("rwx"):
                raise SourceError(
                    f"line {lineno}: invalid segment flags {flag_text!r}")
            flags = sum(bit for name, bit in (("r", 4), ("w", 2), ("x", 1))
                        if name in flag_text)
            if section in sections:
                raise SourceError(f"line {lineno}: duplicate .{section}")
            sections[section] = {"base": base, "data": bytearray(),
                                 "flags": flags, "bss": 0}
            addresses[section] = base
            continue
        if line.startswith(".expect "):
            expectations.append(line[8:].strip())
            continue
        if line.startswith(".mutate "):
            mutations.append(line[8:].split())
            continue
        if section is None:
            raise SourceError(f"line {lineno}: content before .text/.data")

        if ":" in line:
            name, line = (part.strip() for part in line.split(":", 1))
            if not re.fullmatch(r"[a-z_][a-z0-9_]*", name) or name in labels:
                raise SourceError(
                    f"line {lineno}: invalid or duplicate label {name!r}")
            labels[name] = addresses[section]
            if not line:
                continue

        if line.startswith(".align "):
            align = integer(line.split()[1])
            if align <= 0 or align & (align - 1):
                raise SourceError(
                    f"line {lineno}: alignment must be a power of two")
            padding = (-addresses[section]) & (align - 1)
            sections[section]["data"].extend(bytes(padding))
            addresses[section] += padding
        elif line.startswith(".bss "):
            size = checked(integer(line.split()[1]), 0, 0xFFFFFFFF,
                           "BSS size")
            sections[section]["bss"] += size
        elif line.startswith(".byte "):
            values = [checked(integer(x), -128, 255, "byte") & 0xFF
                      for x in split_args(line[6:])]
            sections[section]["data"].extend(values)
            addresses[section] += len(values)
        elif line.startswith(".word "):
            for expression in split_args(line[6:]):
                offset = len(sections[section]["data"])
                try:
                    value = integer(expression) & 0xFFFFFFFF
                except ValueError:
                    value = 0
                    word_fixups.append((lineno, section, offset, expression))
                sections[section]["data"].extend(struct.pack("<I", value))
                addresses[section] += 4
        else:
            if section != "text" or addresses[section] & 3:
                raise SourceError(
                    f"line {lineno}: instruction must be in aligned .text")
            entries.append((lineno, section, len(sections[section]["data"]),
                            addresses[section], line))
            sections[section]["data"].extend(bytes(4))
            addresses[section] += 4

    if "text" not in sections:
        raise SourceError("missing .text")
    if not expectations:
        raise SourceError("missing .expect")
    for lineno, name, offset, pc, text in entries:
        try:
            word = encode_instruction(text, pc, labels)
        except (SourceError, ValueError, KeyError) as exc:
            raise SourceError(f"line {lineno}: {exc}") from exc
        struct.pack_into("<I", sections[name]["data"], offset, word)
    for lineno, name, offset, expression in word_fixups:
        if expression not in labels:
            raise SourceError(f"line {lineno}: unknown label {expression!r}")
        struct.pack_into("<I", sections[name]["data"], offset,
                         labels[expression])
    source_map = {(name, offset): lineno
                  for lineno, name, offset, _pc, _text in entries}
    return sections, expectations, mutations, source_map


def build_elf(sections, mutations):
    ordered = [sections[name] for name in ("text", "data") if name in sections]
    phnum = len(ordered)
    ident = b"\x7fELF" + bytes((1, 1, 1, 0, 0)) + bytes(7)
    ehdr = bytearray(struct.pack("<16sHHIIIIIHHHHHH", ident, 2, 0xFF32, 1,
                                 sections["text"]["base"], 52, 0, 0,
                                 52, 32, phnum, 0, 0, 0))
    phdrs = bytearray()
    for index, item in enumerate(ordered):
        file_offset = 0x1000 * (index + 1)
        data = item["data"]
        phdrs.extend(struct.pack("<IIIIIIII", 1, file_offset, item["base"],
                                 item["base"], len(data),
                                 len(data) + item["bss"],
                                 item["flags"], 0x1000))
    artifact = ehdr + phdrs
    for index, item in enumerate(ordered):
        file_offset = 0x1000 * (index + 1)
        artifact.extend(bytes(file_offset - len(artifact)))
        artifact.extend(item["data"])
    for mutation in mutations:
        field, value = mutation[0], integer(mutation[1])
        offsets = {
            "class": 4, "endianness": 5, "type": 16, "machine": 18,
            "flags": 36, "entry": 24, "entry-alignment": 24,
            "phdr-type": 52, "phdr-offset": 56, "phdr-vaddr": 60,
            "phdr-filesz": 68, "phdr-memsz": 72, "phdr-flags": 76,
            "phdr-align": 80, "phdr2-vaddr": 92,
        }
        if field not in offsets:
            raise SourceError(f"unsupported mutation {field!r}")
        size = (1 if field in ("class", "endianness") else
                2 if field in ("type", "machine") else 4)
        artifact[offsets[field]:offsets[field] + size] = value.to_bytes(
            size, "little")
    return bytes(artifact)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("-o", "--output", type=pathlib.Path, required=True)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--dump", action="store_true")
    args = parser.parse_args()
    try:
        sections, expectations, mutations, source_map = parse_source(
            args.source.read_text(encoding="utf-8"))
        artifact = build_elf(sections, mutations)
        if args.check:
            if not args.output.exists() or args.output.read_bytes() != artifact:
                raise SourceError(f"{args.output}: artifact differs")
        else:
            args.output.write_bytes(artifact)
            args.output.chmod(args.output.stat().st_mode | 0o111)
        if args.dump:
            print("ELF32 little-endian ET_EXEC machine=0xff32")
            print(f"entry=0x{sections['text']['base']:08x} "
                  f"bytes={len(artifact)}")
            for index, name in enumerate(name for name in ("text", "data")
                                         if name in sections):
                item = sections[name]
                print(f"PHDR[{index}] {name} offset=0x{(index + 1) * 0x1000:x} "
                      f"vaddr=0x{item['base']:08x} filesz={len(item['data'])} "
                      f"memsz={len(item['data']) + item['bss']} "
                      f"flags={item['flags']}")
                for offset in range(0, len(item["data"]) - 3, 4):
                    word = struct.unpack_from("<I", item["data"], offset)[0]
                    line = source_map.get((name, offset))
                    source = f"line={line} " if line is not None else ""
                    print(f"{source}0x{item['base'] + offset:08x}: "
                          f"0x{word:08x}")
            print("expect=" + ", ".join(expectations))
    except (OSError, SourceError, ValueError) as exc:
        print(f"q32-artifact: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
