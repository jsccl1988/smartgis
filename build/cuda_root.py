#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Print CUDA toolkit root if nvcc + CUDA headers exist, else empty.

Looks at CUDA_PATH / CUDA_HOME / CUDA_PATH_V*, nvcc on PATH, and the usual
Program Files layouts. CUDA 12.8+ may keep Thrust under include/cccl/thrust.
"""

from __future__ import annotations

import glob
import os
import shutil


def _nvcc_in(root: str) -> str:
    for name in ("nvcc.exe", "nvcc"):
        p = os.path.join(root, "bin", name)
        if os.path.isfile(p):
            return p
    return ""


def _has_headers(root: str) -> bool:
    inc = os.path.join(root, "include")
    runtime = os.path.join(inc, "cuda_runtime.h")
    thrust_a = os.path.join(inc, "thrust", "device_vector.h")
    thrust_b = os.path.join(inc, "cccl", "thrust", "device_vector.h")
    return os.path.isfile(runtime) or os.path.isfile(thrust_a) or os.path.isfile(
        thrust_b
    )


def _ok(root: str) -> bool:
    if not root:
        return False
    root = os.path.normpath(root)
    return bool(_nvcc_in(root) and _has_headers(root))


def _root_from_nvcc(nvcc: str) -> str:
    # .../bin/nvcc.exe → toolkit root
    return os.path.dirname(os.path.dirname(os.path.abspath(nvcc)))


def _candidates() -> list[str]:
    out: list[str] = []
    for key, val in os.environ.items():
        ku = key.upper()
        if ku in ("CUDA_PATH", "CUDA_HOME") or ku.startswith("CUDA_PATH_V"):
            v = val.strip().strip('"')
            if v:
                out.append(v)
    which = shutil.which("nvcc") or shutil.which("nvcc.exe")
    if which:
        out.append(_root_from_nvcc(which))
    for base in (
        r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v*",
        r"C:\Program Files (x86)\NVIDIA GPU Computing Toolkit\CUDA\v*",
        r"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\*",
    ):
        out.extend(sorted(glob.glob(base), reverse=True))
    return out


def main() -> int:
    seen = set()
    for raw in _candidates():
        root = os.path.normpath(raw)
        if root in seen:
            continue
        seen.add(root)
        if _ok(root):
            print(root.replace("\\", "/"))
            return 0
    print("")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
