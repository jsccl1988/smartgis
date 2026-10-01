# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Smoke-parse sample orthogrid3d .vts the same way the VS Code extension does."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def parse_vts_ascii(text: str) -> dict:
    if 'StructuredGrid' not in text:
        raise ValueError("not StructuredGrid")
    m = re.search(r'WholeExtent\s*=\s*"([^"]+)"', text, re.I)
    if not m:
        m = re.search(r'Piece[^>]*Extent\s*=\s*"([^"]+)"', text, re.I)
    if not m:
        raise ValueError("missing extent")
    parts = [int(x) for x in m.group(1).split()]
    nx = parts[1] - parts[0] + 1
    ny = parts[3] - parts[2] + 1
    nz = parts[5] - parts[4] + 1
    pb = re.search(
        r"<Points>[\s\S]*?<DataArray[^>]*>([\s\S]*?)</DataArray>[\s\S]*?</Points>",
        text,
        re.I,
    )
    if not pb:
        raise ValueError("missing Points")
    nums = [float(x) for x in pb.group(1).split()]
    need = nx * ny * nz * 3
    if len(nums) < need:
        raise ValueError(f"point count {len(nums)//3} < {nx*ny*nz}")
    return {"nx": nx, "ny": ny, "nz": nz, "npts": nx * ny * nz}


def main() -> int:
    # .../testing/tools/harness/_shared/scripts/vscode/vscode-vts-preview → repo root
    root = Path(__file__).resolve().parents[7]
    sample = root / "out" / "Debug" / "captures" / "plugin-showcase-orthogrid3d.vts"
    if len(sys.argv) > 1:
        sample = Path(sys.argv[1])
    if not sample.is_file():
        print("missing", sample)
        return 1
    info = parse_vts_ascii(sample.read_text(encoding="utf-8"))
    print(info)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
