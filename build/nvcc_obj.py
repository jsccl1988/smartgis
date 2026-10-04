#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Compile one .cu to a COFF .obj with the CUDA Toolkit nvcc."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--nvcc", required=True)
    p.add_argument("--src", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--include", action="append", default=[])
    p.add_argument("--debug", action="store_true")
    args = p.parse_args()
    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    runtime = "/MDd" if args.debug else "/MD"
    cmd = [
        args.nvcc,
        "-c",
        args.src,
        "-o",
        args.out,
        "-std=c++17",
        "-x",
        "cu",
        "-Xcompiler",
        f"{runtime},/EHsc,/nologo",
    ]
    if args.debug:
        cmd += ["-g", "-G"]
    else:
        cmd += ["-O2"]
    for inc in args.include:
        cmd += ["-I", inc]
    proc = subprocess.run(cmd, check=False)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
