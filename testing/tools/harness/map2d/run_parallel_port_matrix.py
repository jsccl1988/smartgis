# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run leftover rhi2d parallel x port matrix + src/render Views map2d china.

Leftover: gdi_map_paint_test LoadLibrary ports x SMT_RHI2D_PARALLEL.
src/render: SmartGisViews --map2d-showcase=china (Map2dPresenter software
export_bmp + optional FlyCube present_gpu) at the same 1280x720 china frame.

Note: leftover execute_ms is IR replay only (no DEM hillshade / MapFrame layout).
src_render export_ms / present_gpu_* include layout, hillshade, paint/upload.
Compare phase columns, not execute_ms vs export_ms as equal work.
"""

from __future__ import annotations

import csv
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out" / "Debug"
EXE = OUT / "gdi_map_paint_test.exe"
VIEWS = OUT / "SmartGisViews.exe"
MATRIX = OUT / "captures" / "map2d" / "matrix"
PORTS = ("gdi", "gdiplus", "skia")
PARALLELS = ("serial", "tile", "layer")
MS_RE = re.compile(r"SMT_RHI2D_PARALLEL=(\w+)\s+execute_ms=(\d+)")
EXPORT_MS_RE = re.compile(r"map2d-showcase:\s+export_ms=(\d+)")
GPU_MS_RE = re.compile(r"map2d-showcase:\s+present_gpu=\d+\s+present_gpu_ms=(\d+)")
GPU_COLD_RE = re.compile(r"map2d-showcase:\s+.*present_gpu_cold_ms=(\d+)")
GPU_WARM_RE = re.compile(r"map2d-showcase:\s+present_gpu_warm=\d+\s+present_gpu_warm_ms=(\d+)")
PHASE_RE = re.compile(
    r"map2d-showcase:\s+phase_(\w+)\s+"
    r"layout_ms=(-?\d+)\s+hillshade_ms=(-?\d+)\s+"
    r"software_paint_ms=(-?\d+)\s+(?:paint_ms=(-?\d+)\s+)?"
    r"bmp_io_ms=(-?\d+)\s+"
    r"gpu_upload_ms=(-?\d+)\s+gpu_present_ms=(-?\d+)"
)
PAINT_MS_RE = re.compile(r"map2d-showcase:\s+export_ms=\d+\s+paint_ms=(\d+)")
SIZE_RE = re.compile(r"map2d-showcase:\s+mode=\w+\s+size=(\d+)x(\d+)")

PHASE_FIELDS = (
    "layout_ms",
    "hillshade_ms",
    "software_paint_ms",
    "paint_ms",
    "bmp_io_ms",
    "gpu_upload_ms",
    "gpu_present_ms",
)


def _empty_phases() -> dict:
    return {k: None for k in PHASE_FIELDS}


def _parse_phases(log_text: str) -> dict[str, dict]:
    out: dict[str, dict] = {}
    for m in PHASE_RE.finditer(log_text):
        tag = m.group(1)
        soft = int(m.group(4))
        paint = m.group(5)
        out[tag] = {
            "layout_ms": int(m.group(2)),
            "hillshade_ms": int(m.group(3)),
            "software_paint_ms": soft,
            "paint_ms": int(paint) if paint is not None else soft,
            "bmp_io_ms": int(m.group(6)),
            "gpu_upload_ms": int(m.group(7)),
            "gpu_present_ms": int(m.group(8)),
        }
    return out


def _phase_sum(p: dict | None, keys: tuple[str, ...]) -> int | None:
    if not p:
        return None
    return sum(int(p.get(k) or 0) for k in keys)


def _within_pct(total: int | None, wall: int | None, pct: float = 15.0) -> bool | None:
    if total is None or wall is None or wall <= 0:
        return None
    return abs(total - wall) * 100.0 / wall <= pct


def run_leftover(port: str, parallel: str) -> dict:
    MATRIX.mkdir(parents=True, exist_ok=True)
    tag = f"{parallel}_{port}"
    log_path = MATRIX / f"leftover_{tag}.log"
    bmp_path = MATRIX / f"leftover-{tag}.bmp"
    env = os.environ.copy()
    env["SMT_RHI2D_PORT"] = port
    env["SMT_RHI2D_PARALLEL"] = parallel
    env["SMT_RHI2D_PARALLEL_LOG"] = "1"
    env["SMT_RHI2D_MATRIX_BMP"] = str(bmp_path)
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")

    t0 = time.perf_counter()
    proc = subprocess.run(
        [str(EXE)],
        cwd=str(OUT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=300,
    )
    wall_ms = int((time.perf_counter() - t0) * 1000)
    log_text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log_path.write_text(log_text, encoding="utf-8")
    execute_ms = [int(m.group(2)) for m in MS_RE.finditer(log_text)]
    row = {
        "engine": "leftover",
        "port": port,
        "parallel": parallel,
        "rc": proc.returncode,
        "wall_ms": wall_ms,
        "execute_ms_list": execute_ms,
        "execute_ms_last": execute_ms[-1] if execute_ms else None,
        "execute_ms_sum": sum(execute_ms) if execute_ms else None,
        "execute_ms_max": max(execute_ms) if execute_ms else None,
        "export_ms": None,
        "present_gpu_ms": None,
        "present_gpu_cold_ms": None,
        "present_gpu_warm_ms": None,
        "viewport": "1280x720",
        "bmp": str(bmp_path.relative_to(OUT)) if bmp_path.exists() else None,
        "bmp_bytes": bmp_path.stat().st_size if bmp_path.exists() else 0,
        "pass": proc.returncode == 0 and bmp_path.exists(),
        "note": "execute_ms=IR replay only; not comparable to src_render export_ms",
    }
    row.update(_empty_phases())
    return row


def run_src_render() -> dict:
    """Views Map2d china @ 1280x720 — software export + optional GPU present."""
    MATRIX.mkdir(parents=True, exist_ok=True)
    log_path = MATRIX / "src_render_china.log"
    bmp_dst = MATRIX / "src_render-china.bmp"
    env = os.environ.copy()
    env["SMT_MAP2D_SHOWCASE_W"] = "1280"
    env["SMT_MAP2D_SHOWCASE_H"] = "720"
    env["SMT_MAP2D_SHOWCASE_LINGER_MS"] = "0"
    # Exercise src/render FlyCube present when adapter is available.
    env["SMT_MAP2D_SHOWCASE_GPU"] = "1"
    # Bench-only: warm present-cache then blit for equal-profile paint_ms.
    env["SMT_MAP2D_EXPORT_REUSE"] = "1"
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")

    t0 = time.perf_counter()
    proc = subprocess.run(
        [str(VIEWS), "--map2d-showcase=china"],
        cwd=str(OUT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=300,
    )
    wall_ms = int((time.perf_counter() - t0) * 1000)
    log_text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log_path.write_text(log_text, encoding="utf-8")

    export_ms = None
    m = EXPORT_MS_RE.search(log_text)
    if m:
        export_ms = int(m.group(1))
    present_gpu_ms = None
    g = GPU_MS_RE.search(log_text)
    if g:
        present_gpu_ms = int(g.group(1))
    present_gpu_cold_ms = present_gpu_ms
    gc = GPU_COLD_RE.search(log_text)
    if gc:
        present_gpu_cold_ms = int(gc.group(1))
    present_gpu_warm_ms = None
    gw = GPU_WARM_RE.search(log_text)
    if gw:
        present_gpu_warm_ms = int(gw.group(1))

    phases = _parse_phases(log_text)
    export_ph = phases.get("export")
    cold_ph = phases.get("cold_present")
    warm_ph = phases.get("warm_present")

    # CSV phases: export owns software/bmp; cold present owns gpu_*.
    # layout/hillshade are warmed off-clock (ensure_full) so timed samples
    # correctly show 0 — leftover execute_ms remains IR-only.
    phase_row = _empty_phases()
    if export_ph:
        phase_row["layout_ms"] = export_ph.get("layout_ms")
        phase_row["hillshade_ms"] = export_ph.get("hillshade_ms")
        phase_row["software_paint_ms"] = export_ph.get("software_paint_ms")
        phase_row["paint_ms"] = export_ph.get(
            "paint_ms", export_ph.get("software_paint_ms")
        )
        phase_row["bmp_io_ms"] = export_ph.get("bmp_io_ms")
    paint_ms = phase_row.get("paint_ms")
    pm = PAINT_MS_RE.search(log_text)
    if pm:
        paint_ms = int(pm.group(1))
        phase_row["paint_ms"] = paint_ms
    if cold_ph:
        phase_row["gpu_upload_ms"] = cold_ph.get("gpu_upload_ms")
        phase_row["gpu_present_ms"] = cold_ph.get("gpu_present_ms")
        if phase_row["layout_ms"] in (None, 0) and cold_ph.get("layout_ms"):
            phase_row["layout_ms"] = cold_ph.get("layout_ms")
        if phase_row["hillshade_ms"] in (None, 0) and cold_ph.get(
            "hillshade_ms"
        ):
            phase_row["hillshade_ms"] = cold_ph.get("hillshade_ms")

    export_sum = _phase_sum(
        export_ph,
        ("layout_ms", "hillshade_ms", "software_paint_ms", "bmp_io_ms"),
    )
    cold_sum = _phase_sum(
        cold_ph,
        ("layout_ms", "hillshade_ms", "gpu_upload_ms", "gpu_present_ms"),
    )
    phase_gate_export = _within_pct(export_sum, export_ms)
    phase_gate_cold = _within_pct(cold_sum, present_gpu_cold_ms)

    viewport = "1280x720"
    s = SIZE_RE.search(log_text)
    if s:
        viewport = f"{s.group(1)}x{s.group(2)}"

    # Showcase writes next to exe under captures/... or sidecar — prefer keep.
    src_candidates = [
        OUT / "captures" / "map2d" / "map2d-showcase-china.bmp",
        OUT / "captures" / "map2d" / "map2d-showcase-china.keep.bmp",
        OUT / "map2d-showcase-china.bmp",
        OUT / "map2d-showcase-china.keep.bmp",
    ]
    # Also search capture roots under out/Debug/captures.
    for p in (OUT / "captures").rglob("map2d-showcase-china*.bmp"):
        src_candidates.append(p)

    copied = False
    for src in src_candidates:
        if src.is_file() and src.stat().st_size > 10000:
            shutil.copy2(src, bmp_dst)
            copied = True
            break

    row = {
        "engine": "src_render",
        "port": "views+flycube",
        "parallel": "map_effect",
        "rc": proc.returncode,
        "wall_ms": wall_ms,
        "execute_ms_list": [],
        "execute_ms_last": export_ms,
        "execute_ms_sum": export_ms,
        "execute_ms_max": export_ms,
        "export_ms": export_ms,
        "present_gpu_ms": present_gpu_ms,
        "present_gpu_cold_ms": present_gpu_cold_ms,
        "present_gpu_warm_ms": present_gpu_warm_ms,
        "viewport": viewport,
        "bmp": str(bmp_dst.relative_to(OUT)) if bmp_dst.exists() else None,
        "bmp_bytes": bmp_dst.stat().st_size if bmp_dst.exists() else 0,
        "pass": copied and bmp_dst.exists() and bmp_dst.stat().st_size > 10000,
        "phase_sum_export": export_sum,
        "phase_sum_cold_present": cold_sum,
        "phase_gate_export_ok": phase_gate_export,
        "phase_gate_cold_ok": phase_gate_cold,
        "paint_ms": paint_ms,
        "phase_export": export_ph,
        "phase_cold_present": cold_ph,
        "phase_warm_present": warm_ph,
        "note": (
            "leftover execute_ms != src_render export_ms "
            "(IR-only vs layout+hillshade+paint)"
        ),
    }
    row.update(phase_row)
    return row


def write_outputs(rows: list[dict]) -> None:
    summary = MATRIX / "parallel_port_matrix_with_src_render.json"
    summary.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    # Keep legacy leftover-only names for existing consumers.
    leftover_rows = [r for r in rows if r.get("engine") == "leftover"]
    (MATRIX / "leftover_parallel_port_matrix.json").write_text(
        json.dumps(leftover_rows, indent=2), encoding="utf-8"
    )

    fields = [
        "engine",
        "parallel",
        "port",
        "rc",
        "pass",
        "wall_ms",
        "execute_ms_last",
        "execute_ms_sum",
        "execute_ms_max",
        "export_ms",
        "present_gpu_ms",
        "present_gpu_cold_ms",
        "present_gpu_warm_ms",
        *PHASE_FIELDS,
        "viewport",
        "bmp_bytes",
        "bmp",
    ]
    csv_path = MATRIX / "parallel_port_matrix_with_src_render.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k) for k in fields})

    leftover_csv = MATRIX / "leftover_parallel_port_matrix.csv"
    with leftover_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "parallel",
                "port",
                "rc",
                "pass",
                "wall_ms",
                "execute_ms_last",
                "execute_ms_sum",
                "execute_ms_max",
                "bmp_bytes",
                "bmp",
            ],
        )
        w.writeheader()
        for r in leftover_rows:
            w.writerow({k: r.get(k) for k in w.fieldnames})

    print(f"wrote {summary}")
    print(f"wrote {csv_path}")
    print(f"wrote {leftover_csv}")
    print(
        "note: leftover execute_ms is IR replay only; "
        "src_render phases are the fair compare surface"
    )


def main() -> int:
    if not EXE.exists():
        print(f"missing {EXE}; build gdi_map_paint_test first", file=sys.stderr)
        return 2
    rows: list[dict] = []
    for parallel in PARALLELS:
        for port in PORTS:
            print(f"=== leftover {parallel} x {port} ===", flush=True)
            try:
                row = run_leftover(port, parallel)
            except subprocess.TimeoutExpired:
                row = {
                    "engine": "leftover",
                    "port": port,
                    "parallel": parallel,
                    "rc": -1,
                    "wall_ms": 120000,
                    "execute_ms_list": [],
                    "execute_ms_last": None,
                    "execute_ms_sum": None,
                    "execute_ms_max": None,
                    "export_ms": None,
                    "present_gpu_ms": None,
                    "present_gpu_cold_ms": None,
                    "present_gpu_warm_ms": None,
                    "viewport": "1280x720",
                    "bmp": None,
                    "bmp_bytes": 0,
                    "pass": False,
                    "error": "timeout",
                }
                row.update(_empty_phases())
            rows.append(row)
            print(
                f"  rc={row['rc']} wall={row['wall_ms']} "
                f"exec_max={row['execute_ms_max']} bmp={row['bmp']}",
                flush=True,
            )

    if VIEWS.exists():
        print("=== src_render Views map2d china 1280x720 ===", flush=True)
        try:
            row = run_src_render()
        except subprocess.TimeoutExpired:
            row = {
                "engine": "src_render",
                "port": "views+flycube",
                "parallel": "map_effect",
                "rc": -1,
                "wall_ms": 120000,
                "execute_ms_list": [],
                "execute_ms_last": None,
                "execute_ms_sum": None,
                "execute_ms_max": None,
                "export_ms": None,
                "present_gpu_ms": None,
                "present_gpu_cold_ms": None,
                "present_gpu_warm_ms": None,
                "viewport": "1280x720",
                "bmp": None,
                "bmp_bytes": 0,
                "pass": False,
                "error": "timeout",
            }
            row.update(_empty_phases())
        rows.append(row)
        print(
            f"  rc={row['rc']} wall={row['wall_ms']} "
            f"export_ms={row.get('export_ms')} "
            f"paint_ms={row.get('paint_ms')} "
            f"present_gpu_cold_ms={row.get('present_gpu_cold_ms')} "
            f"present_gpu_warm_ms={row.get('present_gpu_warm_ms')} "
            f"layout_ms={row.get('layout_ms')} "
            f"hillshade_ms={row.get('hillshade_ms')} "
            f"software_paint_ms={row.get('software_paint_ms')} "
            f"bmp_io_ms={row.get('bmp_io_ms')} "
            f"gpu_upload_ms={row.get('gpu_upload_ms')} "
            f"gpu_present_ms={row.get('gpu_present_ms')} "
            f"phase_gate_export={row.get('phase_gate_export_ok')} "
            f"phase_gate_cold={row.get('phase_gate_cold_ok')} "
            f"bmp={row['bmp']}",
            flush=True,
        )
    else:
        print(f"skip src_render: missing {VIEWS}", flush=True)

    write_outputs(rows)
    return 0 if all(r.get("pass") for r in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
