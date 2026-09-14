#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build the C# WinUI host into GN's out/ directory."""

import os
import shutil
import subprocess
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: dotnet_build.py <csproj> <out_dir>", file=sys.stderr)
        return 2
    csproj = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    os.makedirs(out_dir, exist_ok=True)
    dotnet = shutil.which("dotnet")
    if not dotnet:
        print("ERROR: dotnet SDK not on PATH. Install .NET 8 SDK.",
              file=sys.stderr)
        return 1
    cmd = [
        dotnet,
        "build",
        csproj,
        "-c",
        "Debug",
        "-o",
        out_dir,
        "--nologo",
        "-v",
        "m",
    ]
    print(" ".join(cmd), flush=True)
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
