#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import pathlib
import os
import struct
import subprocess
import sys
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).with_name("q32-artifact.py")


class Q32ArtifactCLITest(unittest.TestCase):
    def run_tool(self, source, *options):
        with tempfile.TemporaryDirectory() as tmp:
            source_path = pathlib.Path(tmp, "input.q32")
            output_path = pathlib.Path(tmp, "output.elf")
            source_path.write_text(source, encoding="utf-8")
            result = subprocess.run(
                [sys.executable, SCRIPT, source_path, "-o", output_path,
                 *options],
                text=True,
                capture_output=True,
                check=False,
            )
            artifact = output_path.read_bytes() if output_path.exists() else b""
            mode = output_path.stat().st_mode if output_path.exists() else 0
            return result, artifact, mode

    def test_minimal_exit_program_has_deterministic_elf_and_code(self):
        result, artifact, mode = self.run_tool("""\
.text 0x10000
addi a0, zero, 0
addi a7, zero, 93
ecall
.expect exit 0
""")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(mode & os.X_OK)
        self.assertEqual(artifact[:4], b"\x7fELF")
        self.assertEqual(struct.unpack_from("<H", artifact, 18)[0], 0xFF32)
        self.assertEqual(struct.unpack_from("<I", artifact, 24)[0], 0x10000)
        self.assertEqual(
            artifact[0x1000:0x100c],
            bytes.fromhex("2b050000 ab08d005 1f000000"),
        )

    def test_labels_data_and_all_instruction_formats(self):
        result, artifact, _ = self.run_tool("""\
.text 0x10000
start:
lui t0, 0x12
auipc t1, 1
add t2, t0, t1
slli t2, t2, 31
sw t2, 0(sp)
lw a0, 0(sp)
beq a0, t2, done
jal zero, start
done:
addi a7, zero, 93
ecall
.data 0x20000
value: .byte 1, 2
.align 4
.word 0x12345678
.expect exit 0
""")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(struct.unpack_from("<H", artifact, 44)[0], 2)
        self.assertEqual(artifact[0x2000:0x2008], b"\x01\x02\x00\x00xV4\x12")

    def test_rejects_misaligned_branch_target_with_line_number(self):
        result, _, _ = self.run_tool("""\
.text 0x10000
beq zero, zero, 0x10002
.expect exit 0
""")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("line 2", result.stderr.lower())
        self.assertIn("aligned", result.stderr.lower())

    def test_explicit_rwx_and_elf_mutation_are_visible_in_headers(self):
        result, artifact, _ = self.run_tool("""\
.text 0x10000 rwx
nop
.mutate flags 1
.mutate phdr-memsz 0x20
.expect exit 0
""")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(struct.unpack_from("<I", artifact, 36)[0], 1)
        self.assertEqual(struct.unpack_from("<I", artifact, 72)[0], 0x20)
        self.assertEqual(struct.unpack_from("<I", artifact, 76)[0], 7)

    def test_dump_describes_headers_words_and_expectations(self):
        result, _, _ = self.run_tool("""\
.text 0x10000
nop
.expect exit 0
""", "--dump")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ELF32 little-endian ET_EXEC machine=0xff32",
                      result.stdout)
        self.assertIn("PHDR[0] text", result.stdout)
        self.assertIn("line=2 0x00010000: 0x0000002b", result.stdout)
        self.assertIn("expect=exit 0", result.stdout)

    def test_check_accepts_identical_artifact_and_rejects_difference(self):
        source = """\
.text 0x10000
nop
.expect exit 0
"""
        with tempfile.TemporaryDirectory() as tmp:
            source_path = pathlib.Path(tmp, "input.q32")
            output_path = pathlib.Path(tmp, "output.elf")
            source_path.write_text(source, encoding="utf-8")
            create = subprocess.run(
                [sys.executable, SCRIPT, source_path, "-o", output_path],
                text=True, capture_output=True, check=False)
            self.assertEqual(create.returncode, 0, create.stderr)
            check = subprocess.run(
                [sys.executable, SCRIPT, source_path, "-o", output_path,
                 "--check"], text=True, capture_output=True, check=False)
            self.assertEqual(check.returncode, 0, check.stderr)
            output_path.write_bytes(output_path.read_bytes() + b"corrupt")
            check = subprocess.run(
                [sys.executable, SCRIPT, source_path, "-o", output_path,
                 "--check"], text=True, capture_output=True, check=False)
            self.assertNotEqual(check.returncode, 0)
            self.assertIn("artifact differs", check.stderr)

    def test_branch_and_jump_displacement_boundaries(self):
        for encoder, low, high in (("beq zero, zero,", -8192, 8188),
                                   ("jal zero,", -2097152, 2097148)):
            for offset in (low, high):
                result, _, _ = self.run_tool(f"""\
.text 0x10000
{encoder} {0x10000 + offset}
.expect exit 0
""")
                self.assertEqual(result.returncode, 0, result.stderr)
            for offset in (low - 4, high + 4):
                result, _, _ = self.run_tool(f"""\
.text 0x10000
{encoder} {0x10000 + offset}
.expect exit 0
""")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("out of range", result.stderr)


if __name__ == "__main__":
    unittest.main()
