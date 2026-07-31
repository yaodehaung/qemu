#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run one Q32 DSL artifact and enforce its inline expectations."""

import pathlib
import re
import signal
import subprocess
import sys


SIGNALS = {name: value for name, value in signal.__dict__.items()
           if name.startswith("SIG") and "_" not in name}


def expectations(source):
    result = []
    for line in source.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        match = re.fullmatch(r'\.expect\s+(exit|signal|stdout)\s+(.+)', line,
                             re.IGNORECASE)
        if match:
            result.append((match.group(1).lower(), match.group(2).strip()))
    return result


def quoted_string(text):
    if len(text) < 2 or text[0] != '"' or text[-1] != '"':
        raise ValueError("stdout expectation must be a quoted string")
    return bytes(text[1:-1], "utf-8").decode("unicode_escape")


def main():
    if len(sys.argv) != 4:
        print("usage: q32-runner.py QEMU SOURCE ELF", file=sys.stderr)
        return 2
    qemu, source, artifact = sys.argv[1], pathlib.Path(sys.argv[2]), sys.argv[3]
    run = subprocess.run([qemu, artifact], text=True, capture_output=True,
                         check=False)
    mutated = any(line.split("#", 1)[0].strip().lower().startswith(".mutate ")
                  for line in source.read_text(encoding="utf-8").splitlines())
    if mutated:
        loader_errors = ("Invalid ELF", "Error while loading",
                         "Overlapping ELF load segments")
        if run.returncode == 0 or not any(error in run.stderr
                                          for error in loader_errors):
            print("mutated ELF was not rejected by the loader",
                  file=sys.stderr)
            if run.stderr:
                print(run.stderr, file=sys.stderr, end="")
            return 1
        return 0
    failures = []
    for kind, value in expectations(source):
        if kind == "exit" and run.returncode != int(value, 0):
            failures.append(f"exit: expected {value}, got {run.returncode}")
        elif kind == "signal":
            signum = SIGNALS[value.upper()]
            expected = (-signum, 128 + signum)
            if run.returncode not in expected:
                failures.append(f"signal: expected {value} ({expected}), "
                                f"got {run.returncode}")
        elif kind == "stdout" and run.stdout != quoted_string(value):
            failures.append(f"stdout: expected {value}, got {run.stdout!r}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        if run.stderr:
            print(run.stderr, file=sys.stderr, end="")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
