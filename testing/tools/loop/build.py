# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from __future__ import annotations

import subprocess
from pathlib import Path


def build_debug(root: Path, target: str) -> int:
    print(f"BUILD: build.bat debug {target}", flush=True)
    return subprocess.call(
        ["cmd", "/c", "build.bat", "debug", target],
        cwd=str(root),
    )
