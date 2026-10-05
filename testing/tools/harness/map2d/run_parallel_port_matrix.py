# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run Scenic rhi2d parallel x port matrix + Vista map2d china.

Scenic GDI / GDI+ / Skia: scenic_gdi_map_paint_test LoadLibrary
scenic_rhi2d_{gdi,gdiplus,skia} x RHI2D_PARALLEL. Same Scenic engine.
src/legacy/ is frozen and is not a matrix axis.

Vista (codename): SmartGIS.exe --map2d-showcase=china — Map2dPresenter +
gis/vista Layout + effect/map + optional FlyCube present_gpu, same 1280x720
china frame. Engine id in CSV/JSON: ``vista``.

Map2dEngine cell: MAP2D_ENGINE=scenic — content-hosted scenic::Engine
GDI of the same china MapScene (not a GDI+/Skia port peer).

Equal-latitude perf (default): MAP2D_NO_HILLSHADE=1 so Vista does not pay
DEM shade — same carto axis as Scenic rhi2d IR (no hillshade). Compare Scenic
execute_ms (IR replay) vs Vista paint_ms / present_gpu_* phases, not as
identical work units.

FALSE-GAP (normative): scenic rhi2d execute_ms = IR replay only; it is NOT
comparable to Vista paint_ms / export_ms / present_gpu_*. Never claim
execute_ms == export_ms. Readers who treat Scenic IR as "Vista is Nx
slower" are reading a false gap.
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
EXE = OUT / "scenic_gdi_map_paint_test.exe"
VIEWS = OUT / "SmartGIS.exe"
MATRIX = OUT / "captures" / "map2d" / "matrix"
PORTS = ("gdi", "gdiplus", "skia")
PARALLELS = ("serial", "tile", "layer")
MS_RE = re.compile(
    r"(?:RHI2D_PARALLEL=|rhi2d_parallel=)(\w+)\s+execute_ms=(\d+)"
)
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

# Normative labels — keep CSV/JSON/stdout in sync (P3 false-gap).
FALSE_GAP_NOTE = (
    "FALSE-GAP: scenic rhi2d execute_ms = IR replay only; "
    "NOT comparable to Vista paint_ms/export_ms/present_gpu_*; "
    "never claim execute_ms == export_ms"
)
EQUAL_LATITUDE_NOTE = (
    "equal-latitude: MAP2D_NO_HILLSHADE=1 (Vista skips DEM shade; "
    "same carto axis as scenic rhi2d IR which has no hillshade)"
)
MATRIX_NOTE = f"{FALSE_GAP_NOTE}; {EQUAL_LATITUDE_NOTE}"
SCENIC_PORT_ROW_NOTE = (
    f"{FALSE_GAP_NOTE}; {EQUAL_LATITUDE_NOTE}; "
    "compare scenic rhi2d cells on execute_ms_max only; "
    "GDI/GDI+/Skia share the Scenic engine (scenic_rhi2d_*)"
)
VISTA_ROW_NOTE = (
    f"{FALSE_GAP_NOTE}; {EQUAL_LATITUDE_NOTE}; "
    "fair surface = Vista phase columns (paint/present/layout)"
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


def _resolve_layout_parallel(env: dict[str, str]) -> str:
    """Matrix default ON; caller may set VISTA_LAYOUT_PARALLEL=0 to force serial.

    Product emitters may still ignore this until vista wires getenv (see plan P3a TODO).
    """
    raw = env.get("VISTA_LAYOUT_PARALLEL")
    if raw is None or raw == "":
        return "1"
    if raw.lower() in ("0", "false", "off", "no"):
        return "0"
    return "1"


def run_scenic_port(port: str, parallel: str) -> dict:
    """One Scenic rhi2d cell: RHI2D_PORT x RHI2D_PARALLEL."""
    MATRIX.mkdir(parents=True, exist_ok=True)
    tag = f"{parallel}_{port}"
    log_path = MATRIX / f"scenic_{tag}.log"
    bmp_path = MATRIX / f"scenic-{tag}.bmp"
    env = os.environ.copy()
    env["RHI2D_PORT"] = port
    env["RHI2D_PARALLEL"] = parallel
    env["RHI2D_PARALLEL_LOG"] = "1"
    env["RHI2D_MATRIX_BMP"] = str(bmp_path)
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")

    t0 = time.perf_counter()
    proc = subprocess.run(
        [
            str(EXE),
            f"--rhi2d-port={port}",
            f"--rhi2d-parallel={parallel}",
            "--rhi2d-parallel-log=1",
            f"--rhi2d-matrix-bmp={bmp_path}",
        ],
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
        "engine": "scenic",
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
        "note": SCENIC_PORT_ROW_NOTE,
        "matrix_note": MATRIX_NOTE,
    }
    row.update(_empty_phases())
    return row


def run_vista() -> dict:
    """Vista map2d china @ 1280x720 — software export + optional GPU present."""
    MATRIX.mkdir(parents=True, exist_ok=True)
    log_path = MATRIX / "vista_china.log"
    bmp_dst = MATRIX / "vista-china.bmp"
    env = os.environ.copy()
    env["MAP2D_SHOWCASE_W"] = "1280"
    env["MAP2D_SHOWCASE_H"] = "720"
    env["MAP2D_SHOWCASE_LINGER_MS"] = "0"
    # Exercise src/render FlyCube present when adapter is available.
    env["MAP2D_SHOWCASE_GPU"] = "1"
    # Equal-latitude vs Scenic rhi2d IR: no DEM hillshade (IR has none).
    # Override with MAP2D_NO_HILLSHADE=0 to measure shade-on product path.
    if env.get("MAP2D_NO_HILLSHADE") not in ("0", "false", "off"):
        env["MAP2D_NO_HILLSHADE"] = "1"
    # Full software paint (not present-cache blit) so paint_ms is same-axis
    # as Scenic vector work. Set MAP2D_EXPORT_REUSE=1 to force blit bench.
    if "MAP2D_EXPORT_REUSE" not in os.environ:
        env.pop("MAP2D_EXPORT_REUSE", None)
    # P3: request vista layout tess parallel (opt-out VISTA_LAYOUT_PARALLEL=0).
    # Emitters must read this env; until wired, parallel_for still runs by job count.
    layout_parallel = _resolve_layout_parallel(env)
    env["VISTA_LAYOUT_PARALLEL"] = layout_parallel
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")
    env.pop("MAP2D_ENGINE", None)

    t0 = time.perf_counter()
    started = time.time()
    vista_cmd = [
        str(VIEWS),
        "--map2d-showcase=china",
        "--map2d-showcase-w=1280",
        "--map2d-showcase-h=720",
        "--map2d-showcase-gpu=1",
        f"--vista-layout-parallel={layout_parallel}",
    ]
    if env.get("MAP2D_NO_HILLSHADE") not in ("0", "false", "off"):
        vista_cmd.append("--map2d-no-hillshade=1")
    proc = subprocess.run(
        vista_cmd,
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
    # correctly show 0 — Scenic rhi2d execute_ms remains IR-only.
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

    # Reject stale BMPs from a prior crashy run (mtime must be after launch).
    copied = False
    for src in src_candidates:
        if not src.is_file() or src.stat().st_size <= 10000:
            continue
        if src.stat().st_mtime + 1.0 < started:
            continue
        shutil.copy2(src, bmp_dst)
        copied = True
        break

    # Suite accept_nonzero_rc_if_bmp: TerminateProcess races can surface -1
    # after PASS; require fresh BMP + export_ms + present_gpu metrics.
    rc_ok = proc.returncode in (0, -1, 0xFFFFFFFF)
    metrics_ok = export_ms is not None and present_gpu_ms is not None
    pass_ok = (
        copied
        and bmp_dst.exists()
        and bmp_dst.stat().st_size > 10000
        and rc_ok
        and metrics_ok
        and "map2d-showcase: PASS" in log_text
    )

    vista_note = (
        f"{VISTA_ROW_NOTE}; "
        f"VISTA_LAYOUT_PARALLEL={layout_parallel} "
        "(harness sets; product getenv wire = plan P3a / parallel plan V1)"
    )
    row = {
        "engine": "vista",
        "port": "views+flycube",
        "parallel": "map_effect",
        "rc": proc.returncode,
        "wall_ms": wall_ms,
        "execute_ms_list": [],
        # Keep columns populated for CSV width, but label makes clear these
        # are NOT Scenic rhi2d IR execute_ms — they mirror export_ms for layout.
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
        "pass": pass_ok,
        "phase_sum_export": export_sum,
        "phase_sum_cold_present": cold_sum,
        "phase_gate_export_ok": phase_gate_export,
        "phase_gate_cold_ok": phase_gate_cold,
        "paint_ms": paint_ms,
        "phase_export": export_ph,
        "phase_cold_present": cold_ph,
        "phase_warm_present": warm_ph,
        "smt_vista_layout_parallel": layout_parallel,
        "note": vista_note,
        "matrix_note": MATRIX_NOTE,
    }
    row.update(phase_row)
    return row


SCENIC_ROW_NOTE = (
    f"{FALSE_GAP_NOTE}; {EQUAL_LATITUDE_NOTE}; "
    "Scenic Map2dEngine = content-hosted scenic::Engine GDI of the same "
    "china MapScene (not a GDI+/Skia port peer, not Vista MapFrame)"
)


def run_scenic() -> dict:
    """Scenic map2d china @ 1280x720 via MAP2D_ENGINE=scenic."""
    MATRIX.mkdir(parents=True, exist_ok=True)
    log_path = MATRIX / "scenic_china.log"
    bmp_dst = MATRIX / "scenic-china.bmp"
    env = os.environ.copy()
    env["MAP2D_SHOWCASE_W"] = "1280"
    env["MAP2D_SHOWCASE_H"] = "720"
    env["MAP2D_SHOWCASE_LINGER_MS"] = "0"
    env["MAP2D_SHOWCASE_GPU"] = "1"
    env["MAP2D_ENGINE"] = "scenic"
    env["SCENE3D_ENGINE"] = env.get("SCENE3D_ENGINE") or "scenic"
    if env.get("MAP2D_NO_HILLSHADE") not in ("0", "false", "off"):
        env["MAP2D_NO_HILLSHADE"] = "1"
    if "MAP2D_EXPORT_REUSE" not in os.environ:
        env.pop("MAP2D_EXPORT_REUSE", None)
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")

    # Private PE copy + serialize: avoid multi-agent LNK / 0xC0000135 races.
    sys.path.insert(0, str(ROOT / "testing" / "tools"))
    from loop.private_runtime import (  # noqa: E402
        acquire_run_lock,
        prepare_private_views_exe,
        release_run_lock,
        with_private_path,
    )

    lock = OUT.parent / "scratch" / "scenic_review.lock"
    got_lock = acquire_run_lock(lock, timeout_sec=180.0)
    private = prepare_private_views_exe(OUT, tag="scenic_review")
    views_exe = private if private is not None else VIEWS
    if private is not None:
        env = with_private_path(env, OUT, private)
    try:
        t0 = time.perf_counter()
        started = time.time()
        scenic_cmd = [
            str(views_exe),
            "--map2d-showcase=china",
            "--map2d-showcase-w=1280",
            "--map2d-showcase-h=720",
            "--map2d-showcase-gpu=1",
            "--map2d-engine=scenic",
        ]
        if env.get("MAP2D_NO_HILLSHADE") not in ("0", "false", "off"):
            scenic_cmd.append("--map2d-no-hillshade=1")
        proc = subprocess.run(
            scenic_cmd,
            cwd=str(OUT),
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=300,
        )
    finally:
        if got_lock:
            release_run_lock(lock)
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
    paint_ms = None
    pm = PAINT_MS_RE.search(log_text)
    if pm:
        paint_ms = int(pm.group(1))

    scratch_cap = OUT.parent / "scratch" / "scenic_review" / "captures"
    src_candidates = [
        OUT / "captures" / "map2d" / "map2d-showcase-china.bmp",
        OUT / "captures" / "map2d" / "map2d-showcase-china.keep.bmp",
        scratch_cap / "map2d" / "map2d-showcase-china.bmp",
        scratch_cap / "map2d" / "map2d-showcase-china.keep.bmp",
        OUT / "map2d-showcase-china.bmp",
        OUT / "map2d-showcase-china.keep.bmp",
    ]
    for root in (OUT / "captures", scratch_cap):
        if root.is_dir():
            for p in root.rglob("map2d-showcase-china*.bmp"):
                src_candidates.append(p)
    copied = False
    for src in src_candidates:
        if not src.is_file() or src.stat().st_size <= 10000:
            continue
        if src.stat().st_mtime + 1.0 < started:
            continue
        shutil.copy2(src, bmp_dst)
        copied = True
        break

    rc_ok = proc.returncode in (0, -1, 0xFFFFFFFF)
    pass_ok = (
        copied
        and bmp_dst.exists()
        and bmp_dst.stat().st_size > 10000
        and rc_ok
        and export_ms is not None
        and "map2d-showcase: PASS" in log_text
    )
    row = {
        "engine": "scenic_engine",
        "port": "content+gdi",
        "parallel": "serial",
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
        "viewport": "1280x720",
        "bmp": str(bmp_dst.relative_to(OUT)) if bmp_dst.exists() else None,
        "bmp_bytes": bmp_dst.stat().st_size if bmp_dst.exists() else 0,
        "pass": pass_ok,
        "paint_ms": paint_ms,
        "note": SCENIC_ROW_NOTE,
        "matrix_note": MATRIX_NOTE,
    }
    row.update(_empty_phases())
    if paint_ms is not None:
        row["paint_ms"] = paint_ms
        row["software_paint_ms"] = paint_ms
    return row


def _fmt_ms(v) -> str:
    if v is None:
        return "-"
    return str(v)


def print_comparison_tables(rows: list[dict]) -> None:
    """Print Scenic rhi2d grid + Vista phase table + Map2dEngine + false-gap."""
    scenic_ports = [
        r
        for r in rows
        if r.get("engine") == "scenic" and r.get("port") in PORTS
    ]
    vista = next((r for r in rows if r.get("engine") == "vista"), None)
    scenic_engine = next(
        (r for r in rows if r.get("engine") == "scenic_engine"), None
    )

    print()
    print("=" * 72)
    print("FALSE-GAP / equal-latitude (read before comparing columns)")
    print("-" * 72)
    print(MATRIX_NOTE)
    print("=" * 72)

    print()
    print(
        "### A) Scenic rhi2d -- parallel x port "
        "(execute_ms_max = IR replay; GDI/GDI+/Skia = same Scenic engine)"
    )
    print(
        f"{'parallel':<10} {'gdi':>10} {'gdiplus':>10} {'skia':>10}  "
        f"(wall_ms / pass)"
    )
    by = {(r.get("parallel"), r.get("port")): r for r in scenic_ports}
    for parallel in PARALLELS:
        cells = []
        walls = []
        for port in PORTS:
            r = by.get((parallel, port))
            if not r:
                cells.append("-")
                walls.append("-")
                continue
            cells.append(_fmt_ms(r.get("execute_ms_max")))
            walls.append(
                f"{_fmt_ms(r.get('wall_ms'))}/{'Y' if r.get('pass') else 'N'}"
            )
        print(
            f"{parallel:<10} {cells[0]:>10} {cells[1]:>10} {cells[2]:>10}  "
            f"({walls[0]}, {walls[1]}, {walls[2]})"
        )
    print(
        "note: scenic rhi2d execute_ms = IR only -- do NOT subtract from Vista "
        "paint/present to claim a product gap"
    )

    print()
    print("### B) Vista phases (fair compare surface vs Scenic IR)")
    if not vista:
        print("(no vista row -- SmartGIS.exe missing or skipped)")
    else:
        metrics = [
            ("wall_ms", "process wall (software+GPU matrix cell)"),
            ("export_ms", "software export incl bmp IO"),
            ("paint_ms", "paint only (excl IO when present)"),
            ("present_gpu_cold_ms", "first FlyCube present"),
            ("present_gpu_warm_ms", "StaticReuse warm"),
            ("layout_ms", "MapFrame / frame-cache layout"),
            ("hillshade_ms", "DEM shade (0 under NO_HILLSHADE)"),
            ("software_paint_ms", "CPU paint into bitmap"),
            ("bmp_io_ms", "BMP write"),
            ("gpu_upload_ms", "upload to FlyCube"),
            ("gpu_present_ms", "GPU present slice"),
            ("smt_vista_layout_parallel", "env set by harness (1=request parallel)"),
            ("pass", "matrix cell pass"),
        ]
        print(f"{'metric':<28} {'ms/value':>12}  notes")
        print("-" * 72)
        for key, notes in metrics:
            print(f"{key:<28} {_fmt_ms(vista.get(key)):>12}  {notes}")
        print()
        print(
            "scenic vs vista: use table A for IR parallel x port; use table B "
            "for product phases -- columns are different work units (FALSE-GAP)"
        )

    print()
    print(
        "### C) Scenic Map2dEngine (MAP2D_ENGINE=scenic; "
        "not a GDI+/Skia port peer)"
    )
    if not scenic_engine:
        print("(no scenic_engine row -- SmartGIS.exe missing or skipped)")
        return
    print(f"{'metric':<28} {'ms/value':>12}  notes")
    print("-" * 72)
    for key, notes in [
        ("wall_ms", "process wall"),
        ("export_ms", "scenic GDI export + BMP IO"),
        ("paint_ms", "scenic paint when logged"),
        ("present_gpu_ms", "present_gpu wall (scenic present, not FlyCube Pass)"),
        ("pass", "BMP + showcase PASS"),
        ("bmp", "captures/map2d/matrix/scenic-china.bmp"),
    ]:
        print(f"{key:<28} {_fmt_ms(scenic_engine.get(key)):>12}  {notes}")
    print(SCENIC_ROW_NOTE)


def write_outputs(rows: list[dict]) -> None:
    summary = MATRIX / "parallel_port_matrix_with_vista.json"
    payload = {
        "matrix_note": MATRIX_NOTE,
        "false_gap_note": FALSE_GAP_NOTE,
        "equal_latitude_note": EQUAL_LATITUDE_NOTE,
        "rows": rows,
    }
    summary.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    # Scenic rhi2d port grid (GDI / GDI+ / Skia) for consumers.
    scenic_port_rows = [
        r
        for r in rows
        if r.get("engine") == "scenic" and r.get("port") in PORTS
    ]
    (MATRIX / "scenic_parallel_port_matrix.json").write_text(
        json.dumps(scenic_port_rows, indent=2), encoding="utf-8"
    )

    note_path = MATRIX / "parallel_port_matrix_NOTE.txt"
    note_path.write_text(MATRIX_NOTE + "\n", encoding="utf-8")

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
        "smt_vista_layout_parallel",
        "note",
    ]
    csv_path = MATRIX / "parallel_port_matrix_with_vista.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k) for k in fields})

    scenic_csv = MATRIX / "scenic_parallel_port_matrix.csv"
    with scenic_csv.open("w", newline="", encoding="utf-8") as f:
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
                "note",
            ],
        )
        w.writeheader()
        for r in scenic_port_rows:
            w.writerow({k: r.get(k) for k in w.fieldnames})

    print(f"wrote {summary}")
    print(f"wrote {csv_path}")
    print(f"wrote {scenic_csv}")
    print(f"wrote {note_path}")
    print(f"matrix_note: {MATRIX_NOTE}")
    print_comparison_tables(rows)


def main() -> int:
    # Prefer Debug; fall back to Release if Debug host missing.
    global OUT, EXE, VIEWS, MATRIX
    if not EXE.exists() and (
        ROOT / "out" / "Release" / "scenic_gdi_map_paint_test.exe"
    ).exists():
        OUT = ROOT / "out" / "Release"
        EXE = OUT / "scenic_gdi_map_paint_test.exe"
        VIEWS = OUT / "SmartGIS.exe"
        MATRIX = OUT / "captures" / "map2d" / "matrix"
        print(f"using Release out: {OUT}", flush=True)

    if not EXE.exists():
        print(
            f"missing {EXE}; build scenic_gdi_map_paint_test first",
            file=sys.stderr,
        )
        return 2
    rows: list[dict] = []
    for parallel in PARALLELS:
        for port in PORTS:
            print(f"=== scenic rhi2d {parallel} x {port} ===", flush=True)
            try:
                row = run_scenic_port(port, parallel)
            except subprocess.TimeoutExpired:
                row = {
                    "engine": "scenic",
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
                    "note": SCENIC_PORT_ROW_NOTE,
                    "matrix_note": MATRIX_NOTE,
                }
                row.update(_empty_phases())
            rows.append(row)
            print(
                f"  rc={row['rc']} wall={row['wall_ms']} "
                f"exec_max={row['execute_ms_max']} bmp={row['bmp']} "
                f"[Scenic IR-only; not Vista paint/present]",
                flush=True,
            )

    if VIEWS.exists():
        print(
            "=== vista map2d china 1280x720 (Views+FlyCube) "
            f"VISTA_LAYOUT_PARALLEL="
            f"{_resolve_layout_parallel(os.environ.copy())} ===",
            flush=True,
        )
        try:
            row = run_vista()
        except subprocess.TimeoutExpired:
            row = {
                "engine": "vista",
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
                "note": VISTA_ROW_NOTE,
                "matrix_note": MATRIX_NOTE,
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
            f"layout_parallel={row.get('smt_vista_layout_parallel')} "
            f"bmp={row['bmp']}",
            flush=True,
        )
        print(
            "=== scenic map2d china 1280x720 (content scenic::Engine) "
            "MAP2D_ENGINE=scenic ===",
            flush=True,
        )
        try:
            srow = run_scenic()
        except subprocess.TimeoutExpired:
            srow = {
                "engine": "scenic_engine",
                "port": "content+gdi",
                "parallel": "serial",
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
                "note": SCENIC_ROW_NOTE,
                "matrix_note": MATRIX_NOTE,
            }
            srow.update(_empty_phases())
        rows.append(srow)
        print(
            f"  rc={srow['rc']} wall={srow['wall_ms']} "
            f"export_ms={srow.get('export_ms')} "
            f"paint_ms={srow.get('paint_ms')} "
            f"present_gpu_ms={srow.get('present_gpu_ms')} "
            f"bmp={srow['bmp']} pass={srow.get('pass')}",
            flush=True,
        )
    else:
        print(f"skip vista: missing {VIEWS}", flush=True)

    write_outputs(rows)
    return 0 if all(r.get("pass") for r in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
