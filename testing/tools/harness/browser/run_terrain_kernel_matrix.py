# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Dataset × scheme DEM kernel matrix (SIMD + base::execution).

Builds/runs //src/vista/terrain:dem_kernel_benchmark and prints the shade
pivot plus artifact paths.

  python testing/tools/harness/browser/run_terrain_kernel_matrix.py
  python testing/tools/harness/browser/run_terrain_kernel_matrix.py --no-build

Artifacts: out/<config>/captures/analysis/terrain_kernel/{matrix.json,matrix.md}
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out" / "Debug"
ART = OUT / "captures" / "analysis" / "terrain_kernel"
EXE = OUT / "dem_kernel_benchmark.exe"
TARGET = "//src/vista/terrain:dem_kernel_benchmark"


def run(cmd: list[str], cwd: Path | None = None) -> int:
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd, cwd=str(cwd or ROOT))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--no-build",
        action="store_true",
        help="Skip build.bat debug dem_kernel_benchmark",
    )
    ap.add_argument(
        "--config",
        choices=("Debug", "Release"),
        default="Debug",
    )
    args = ap.parse_args()

    global OUT, ART, EXE
    OUT = ROOT / "out" / args.config
    ART = OUT / "captures" / "analysis" / "terrain_kernel"
    EXE = OUT / "dem_kernel_benchmark.exe"

    if not args.no_build:
        rc = run(
            [
                "cmd",
                "/c",
                f"build.bat {args.config.lower()} {TARGET}",
            ]
        )
        if rc != 0:
            return rc

    if not EXE.is_file():
        print(f"missing {EXE}", file=sys.stderr)
        return 1

    ART.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["BAKE_DISK"] = "0"
    env["BAKE_BENCH"] = "1"
    rc = subprocess.call([str(EXE)], env=env, cwd=str(OUT))
    matrix = ART / "matrix.json"
    if matrix.is_file():
        data = json.loads(matrix.read_text(encoding="utf-8"))
        cases = data.get("cases", [])
        print(f"\ncases={len(cases)} simd_runtime={data.get('simd_runtime')}")
        print(f"artifacts: {ART}")
        md = ART / "matrix.md"
        if md.is_file():
            print(md.read_text(encoding="utf-8")[:4000])
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
