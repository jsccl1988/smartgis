# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Equal-profile DEM/hillshade bake bench: CPU parallel_for vs CUDA Thrust.

Locked: china_dem, max_edge=768, illumination 335/32, exaggeration 0.5.
PIP sibling: land_mask_test 320x200 x 80 rings.

For the full dataset × scheme matrix (SIMD + execution), prefer
run_terrain_kernel_matrix.py.

Writes out/<config>/captures/analysis/hillshade_bake/bake_bench.json
and bake_bench.md (table). Not a gen-root script.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out" / "Debug"
ART = OUT / "captures" / "analysis" / "hillshade_bake"


def run_test(exe: Path, env: dict[str, str]) -> int:
    if not exe.is_file():
        print(
            f"missing {exe} — build.bat debug "
            "//src/vista/terrain:dem_raster_test",
            file=sys.stderr,
        )
        return 1
    merged = os.environ.copy()
    merged.update(env)
    print(f"+ {exe.name}", flush=True)
    return subprocess.call([str(exe)], env=merged, cwd=str(OUT))


def load_json(name: str) -> dict:
    path = ART / name
    if not path.is_file():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    ART.mkdir(parents=True, exist_ok=True)
    env = {
        "BAKE_BENCH": "1",
        "BAKE_DISK": "0",
        "BAKE_PROFILE": "1",
    }
    rc = 0
    rc |= run_test(OUT / "land_mask_test.exe", env)
    rc |= run_test(OUT / "dem_raster_test.exe", env)
    shade = load_json("shade_kernel.json")
    mask = load_json("land_mask.json")
    report = {
        "profile": shade.get(
            "profile",
            "china_dem max_edge=768 illum=335/32 exag=0.5",
        ),
        "dem_path": shade.get("dem_path", ""),
        "note": (
            "CUDA cell ok=0 is skip (no Toolkit/device), not a test fail. "
            "CPU cell must not include CUDA context. "
            "Map2dPhaseSample.hillshade_ms is layout lump, not this matrix. "
            "Full dataset×scheme matrix: run_terrain_kernel_matrix.py."
        ),
        "kernel": shade.get("kernel", []),
        "slot": shade.get("slot", []),
        "land_mask": mask.get("rows", []),
    }
    (ART / "bake_bench.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    lines = [
        "| cell | backend | used_cuda | cold_ms | warm_ms | extra |",
        "| --- | --- | ---: | ---: | ---: | --- |",
    ]
    for row in report["kernel"]:
        lines.append(
            "| shade_dem_rgba | {backend} | {used_cuda} | {cold_ms} | "
            "{warm_ms} | ok={ok} {w}x{h} |".format(
                w=row.get("w", 0),
                h=row.get("h", 0),
                **row,
            )
        )
    for row in report["slot"]:
        lines.append(
            "| bake_hillshade_slot | {backend} | {used_cuda} | {shade_ms} | "
            "- | load_ms={load_ms} store_ms={store_ms} |".format(
                backend=row.get("backend", "?"),
                used_cuda=row.get("used_cuda", 0),
                shade_ms=row.get("shade_ms", 0),
                load_ms=row.get("load_ms", 0),
                store_ms=row.get("store_ms", 0),
            )
        )
    for row in report["land_mask"]:
        cell = {
            "backend": row.get("backend", "?"),
            "used_cuda": row.get("used_cuda", 0),
            "cold_ms": row.get("cold_ms", 0),
            "warm_ms": row.get("warm_ms", 0),
            "ok": row.get("ok", row.get("used_cuda", 0)),
        }
        lines.append(
            "| fill_lonlat_mask | {backend} | {used_cuda} | {cold_ms} | "
            "{warm_ms} | ok={ok} |".format(**cell)
        )
    md = (
        "# hillshade bake equal-profile\n\n"
        + report["note"]
        + "\n\n"
        + "\n".join(lines)
        + "\n"
    )
    (ART / "bake_bench.md").write_text(md, encoding="utf-8")
    print(md)
    print(f"wrote {ART / 'bake_bench.json'}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
