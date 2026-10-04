# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run plugin.world3d equal-profile backend/parallel matrix.

Same China DEM Scene3D on SmartGIS.exe --plugin-showcase=world3d with
SMT_PLUGIN_WORLD3D_PERF_BARE=1 (sky/ocean/cloud/fog + pointcloud overlay
off; pump_ms=0). Primary metric is warm ms_per_present (discard first cold
frame; n=5). Scenic rhi3d GL + D3D11 use the same Views entry + present
count/discard (SMT_SCENE3D_ENGINE=stereo_gl|stereo_d3d). Leftover/legacy
hosts are frozen — do not launch SmartGIS-Legacy.exe.

Artifacts: out/Debug/captures/analysis/world3d_opt/matrix/
"""

from __future__ import annotations

import csv
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "out" / "Debug"
VIEWS = OUT / "SmartGIS.exe"
MATRIX = OUT / "captures" / "analysis" / "world3d_opt" / "matrix"
PLUGIN_CAP = OUT / "captures" / "plugin"
BMP_LEAF = "plugin-showcase-world3d.bmp"
MARK_LEAF = "plugin-showcase-mark.txt"

# FlyCube perf rows: DEM-only bare profile (no atmo / pointcloud overlay).
_FLYCUBE_PERF_ENV: dict[str, str | None] = {
    "SMT_PLUGIN_WORLD3D_GPU": "1",
    "SMT_PLUGIN_WORLD3D_PERF_BARE": "1",
    "SMT_PREFER_GDI_DEVICE": None,
    "SMT_SCENE3D_ENGINE": None,
}

_SCENIC_GPU_PERF_ENV: dict[str, str | None] = {
    **_FLYCUBE_PERF_ENV,
    "SMT_GPUSCENE_PREP_PARALLEL": None,
    "SMT_RHI3D_FRAME_JOB": "0",
    "SMT_RHI3D_PREP_PARALLEL": "0",
}

# row_id, backend_label, parallel_label, kind, role, env overlays
# kind: views_world3d
# role: perf (performance table) | smoke (run + gate only; not a perf peer)
# leftover/legacy hosts are frozen — GL/D3D peers are scenic rhi3d on Views.
ROWS: list[tuple[str, str, str, str, str, dict[str, str | None]]] = [
    # Views world3d: leave SMT_SCENE3D_ENGINE unset so plugin-showcase keeps
    # the GDI shell default; FlyCube is acquired on the showcase HWND only.
    # GDI is not a 3D GPU peer — omitted from this matrix.
    (
        "flycube",
        "FlyCube/DX12",
        "prep_default",
        "views_world3d",
        "perf",
        {
            **_FLYCUBE_PERF_ENV,
            "SMT_GPUSCENE_PREP_PARALLEL": None,
        },
    ),
    (
        "prep_par_off",
        "FlyCube/DX12",
        "prep_0",
        "views_world3d",
        "perf",
        {
            **_FLYCUBE_PERF_ENV,
            "SMT_GPUSCENE_PREP_PARALLEL": "0",
            # Cull stays off: prep_cull_meshes is a no-op without frustum, so
            # prep_par alone would not exercise the parallel path.
            "SMT_SCENE3D_FRUSTUM_CULL": None,
        },
    ),
    (
        "prep_par_on",
        "FlyCube/DX12",
        "prep_on",
        "views_world3d",
        "perf",
        {
            **_FLYCUBE_PERF_ENV,
            "SMT_GPUSCENE_PREP_PARALLEL": "1",
            # Honesty: parallel prep only runs when frustum cull is active
            # (see effect/scene/detail/prep_cull.cc).
            "SMT_SCENE3D_FRUSTUM_CULL": "1",
        },
    ),
    (
        "null",
        "Null",
        "gpu_off",
        "views_world3d",
        "smoke",
        {
            # Smoke keeps full product materials (not bare).
            "SMT_PLUGIN_WORLD3D_GPU": "0",
            "SMT_PLUGIN_WORLD3D_PERF_BARE": None,
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_GPUSCENE_PREP_PARALLEL": None,
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "gl_scenic",
        "Scenic/GL",
        "scenic_serial",
        "views_world3d",
        "perf",
        {
            **_SCENIC_GPU_PERF_ENV,
            "SMT_SCENE3D_ENGINE": "stereo_gl",
            "SMT_STEREO_API": "OpenGL",
            "SMT_SCENE3D_SHOWCASE_D3D": "0",
        },
    ),
    (
        "d3d_scenic",
        "Scenic/D3D11",
        "scenic_serial",
        "views_world3d",
        "perf",
        {
            **_SCENIC_GPU_PERF_ENV,
            "SMT_SCENE3D_ENGINE": "stereo_d3d",
            "SMT_STEREO_API": "Direct3D",
            "SMT_SCENE3D_SHOWCASE_D3D": "1",
            "SMT_RHI3D_D3D_DEFERRED": "0",
        },
    ),
    (
        "scenic",
        "Scenic/GDI",
        "content_host",
        "views_world3d",
        "scenic",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "1",
            "SMT_PLUGIN_WORLD3D_PERF_BARE": "1",
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_SCENE3D_ENGINE": "scenic",
            "SMT_GPUSCENE_PREP_PARALLEL": None,
        },
    ),
]


def _apply_env(base: dict[str, str], overlay: dict[str, str | None]) -> dict[str, str]:
    env = base.copy()
    for k, v in overlay.items():
        if v is None:
            env.pop(k, None)
        else:
            env[k] = v
    env["SMT_SKIP_MAP_CONTEXT_MENU"] = "1"
    env["SMT_SYNC_FLYCUBE_INIT"] = env.get("SMT_SYNC_FLYCUBE_INIT", "1")
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")
    # PYTHONUNBUFFERED so matrix progress appears while a row runs.
    env["PYTHONUNBUFFERED"] = "1"
    return env


def _kill_showcase_procs() -> None:
    """Drop PE locks / DXGI adapters between matrix rows."""
    if os.name != "nt":
        return
    for name in ("SmartGIS.exe", "SmartGIS-Legacy.exe"):
        subprocess.run(
            ["taskkill", "/IM", name, "/T"],
            capture_output=True,
            text=True,
            check=False,
        )
    time.sleep(2.0)
    for name in ("SmartGIS.exe", "SmartGIS-Legacy.exe"):
        subprocess.run(
            ["taskkill", "/IM", name, "/F", "/T"],
            capture_output=True,
            text=True,
            check=False,
        )
    time.sleep(1.5)


def _read_marks(path: Path) -> list[str]:
    if not path.is_file():
        return []
    text = path.read_text(encoding="utf-8", errors="replace")
    return [ln.strip() for ln in text.splitlines() if ln.strip()]


def _fresh_file(candidates: list[Path], min_mtime: float | None) -> Path | None:
    best: Path | None = None
    best_m = -1.0
    for p in candidates:
        if not p.is_file() or p.stat().st_size <= 0:
            continue
        m = p.stat().st_mtime
        if min_mtime is not None and m < min_mtime - 1.0:
            continue
        if m > best_m:
            best = p
            best_m = m
    return best


def _load_perf_json(src: Path | None, dest_dir: Path) -> dict:
    """Harvest warm ms_per_present (+ cold/all + optional phase fields)."""
    out: dict = {
        "ms_per_present": None,
        "ms_per_present_all": None,
        "ms_per_present_cold": None,
        "present_ms": None,
        "present_ms_warm": None,
        "present_count": None,
        "discard_cold": None,
        "warm_count": None,
        "frame_ms": None,
        "perf_json": None,
        "mesh_ms": None,
        "sync_ms": None,
        "rebuild_ms": None,
        "rebuild_count": None,
        "ocean_prep_ms": None,
        "record_ms": None,
        "present_swap_ms": None,
        "upload_ms": None,
        "pso_ms": None,
        "dem_load_ms": None,
        "tess_ms": None,
        "hypso_ms": None,
        "cold_phase": None,
    }
    if not src or not src.is_file():
        return out
    dest_dir.mkdir(parents=True, exist_ok=True)
    dst = dest_dir / src.name
    _copy2_retry(src, dst)
    out["perf_json"] = str(dst.relative_to(OUT)).replace("\\", "/")
    try:
        data = json.loads(dst.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as ex:
        print(f"world3d-matrix: perf json parse fail {src}: {ex}", file=sys.stderr)
        return out
    for key in (
        "ms_per_present",
        "ms_per_present_all",
        "ms_per_present_cold",
        "present_ms",
        "present_ms_warm",
        "present_count",
        "discard_cold",
        "warm_count",
        "frame_ms",
        "mesh_ms",
        "sync_ms",
        "rebuild_ms",
        "rebuild_count",
        "ocean_prep_ms",
        "record_ms",
        "present_swap_ms",
        "upload_ms",
        "pso_ms",
        "dem_load_ms",
        "tess_ms",
        "hypso_ms",
        "cold_phase",
    ):
        if key in data:
            out[key] = data[key]
    cold = data.get("cold_phase")
    if isinstance(cold, dict):
        for key in (
            "dem_load_ms",
            "tess_ms",
            "hypso_ms",
            "upload_ms",
            "pso_ms",
        ):
            if out.get(key) is None and key in cold:
                out[key] = cold[key]
    return out


def _try_inspect_png(bmp: Path) -> str | None:
    if not bmp.is_file():
        return None
    try:
        sys.path.insert(0, str(ROOT))
        from testing.tools.loop.review.inspect_png import bmp_to_inspect_png

        png = bmp_to_inspect_png(bmp)
        if png:
            return str(Path(png).relative_to(OUT)).replace("\\", "/")
    except Exception as ex:  # noqa: BLE001
        print(f"world3d-matrix: inspect_png fail {bmp}: {ex}", file=sys.stderr)
    return None


def _copy2_retry(src: Path, dst: Path) -> None:
    """Copy on Windows even when Explorer/AV briefly locks the dest."""
    if dst.exists() and src.resolve() == dst.resolve():
        return
    last: BaseException | None = None
    for i in range(8):
        try:
            shutil.copy2(src, dst)
            return
        except (PermissionError, OSError) as ex:
            last = ex
            winerr = getattr(ex, "winerror", None)
            if winerr not in (5, 32, None) and not isinstance(ex, PermissionError):
                break
            time.sleep(0.25 * (i + 1))
    data = src.read_bytes()
    dst.write_bytes(data)
    try:
        shutil.copystat(src, dst)
    except OSError:
        pass
    if last is not None and not dst.is_file():
        raise last


def _under_matrix(path: Path) -> bool:
    try:
        path.resolve().relative_to(MATRIX.resolve())
        return True
    except (OSError, ValueError):
        return False


def _live_capture_hits(leaf: str, *, min_mtime: float | None) -> list[Path]:
    """Plugin-cap first; never harvest prior matrix copies as live artifacts."""
    hits: list[Path] = []
    preferred = PLUGIN_CAP / leaf
    if preferred.is_file():
        hits.append(preferred)
    for p in (OUT / "captures").rglob(leaf):
        if _under_matrix(p):
            continue
        if p not in hits:
            hits.append(p)
    if min_mtime is None:
        return hits
    fresh = [p for p in hits if p.stat().st_mtime >= min_mtime - 1.0]
    return fresh


def _copy_into_row(row_id: str, src_bmp: Path | None, src_mark: Path | None,
                   *, min_mtime: float | None = None) -> dict:
    dest_dir = MATRIX / row_id
    dest_dir.mkdir(parents=True, exist_ok=True)
    out: dict = {"bmp": None, "bmp_bytes": 0, "marks": [], "inspect_png": None}
    if src_bmp and src_bmp.is_file() and src_bmp.stat().st_size > 0:
        if min_mtime is not None and src_bmp.stat().st_mtime < min_mtime - 1.0:
            # Stale leftover from a prior matrix row — do not count as pass.
            src_bmp = None
    if src_bmp and src_bmp.is_file() and src_bmp.stat().st_size > 0:
        dst = dest_dir / src_bmp.name
        _copy2_retry(src_bmp, dst)
        out["bmp"] = str(dst.relative_to(OUT)).replace("\\", "/")
        out["bmp_bytes"] = dst.stat().st_size
        out["inspect_png"] = _try_inspect_png(dst)
    if src_mark and src_mark.is_file():
        if min_mtime is not None and src_mark.stat().st_mtime < min_mtime - 1.0:
            src_mark = None
    if src_mark and src_mark.is_file():
        dst_m = dest_dir / src_mark.name
        _copy2_retry(src_mark, dst_m)
        out["marks"] = _read_marks(dst_m)
    return out


def _find_bmp(candidates: list[Path]) -> Path | None:
    for p in candidates:
        if p.is_file() and p.stat().st_size > 10000:
            return p
    for p in candidates:
        if p.is_file() and p.stat().st_size > 0:
            return p
    return None


def run_views_world3d(row_id: str, backend: str, parallel: str,
                      overlay: dict[str, str | None]) -> dict:
    if not VIEWS.is_file():
        return {
            "row_id": row_id,
            "engine": "plugin.world3d",
            "backend": backend,
            "parallel": parallel,
            "rc": None,
            "wall_ms": None,
            "ms_per_present": None,
            "present_ms": None,
            "present_count": None,
            "bmp": None,
            "bmp_bytes": 0,
            "inspect_png": None,
            "marks": [],
            "pass": False,
            "log": None,
            "note": f"missing {VIEWS.name}",
        }

    MATRIX.mkdir(parents=True, exist_ok=True)
    log_path = MATRIX / f"{row_id}.log"
    _kill_showcase_procs()
    if not VIEWS.is_file():
        return {
            "row_id": row_id,
            "engine": "plugin.world3d",
            "backend": backend,
            "parallel": parallel,
            "rc": None,
            "wall_ms": None,
            "ms_per_present": None,
            "present_ms": None,
            "present_count": None,
            "bmp": None,
            "bmp_bytes": 0,
            "inspect_png": None,
            "marks": [],
            "pass": False,
            "log": None,
            "note": f"missing {VIEWS.name} after kill",
        }
    env = _apply_env(os.environ.copy(), overlay)
    views_exe = VIEWS
    lock_path = OUT.parent / "scratch" / "scenic_review.lock"
    got_lock = False
    scenic_row = (overlay.get("SMT_SCENE3D_ENGINE") or "").lower() == "scenic"
    if scenic_row:
        sys.path.insert(0, str(ROOT / "testing" / "tools"))
        from loop.private_runtime import (  # noqa: E402
            acquire_run_lock,
            prepare_private_views_exe,
            release_run_lock,
            with_private_path,
        )

        got_lock = acquire_run_lock(lock_path, timeout_sec=180.0)
        private = prepare_private_views_exe(OUT, tag="scenic_review")
        if private is not None:
            views_exe = private
            env = with_private_path(env, OUT, private)

    t0 = time.perf_counter()
    t0_wall = time.time()
    try:
        try:
            proc = subprocess.run(
                [str(views_exe), "--plugin-showcase=world3d"],
                cwd=str(OUT),
                env=env,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=240,
            )
        except FileNotFoundError:
            return {
                "row_id": row_id,
                "engine": "plugin.world3d",
                "backend": backend,
                "parallel": parallel,
                "rc": None,
                "wall_ms": None,
                "ms_per_present": None,
                "present_ms": None,
                "present_count": None,
                "bmp": None,
                "bmp_bytes": 0,
                "inspect_png": None,
                "marks": [],
                "pass": False,
                "log": None,
                "note": f"CreateProcess missing {views_exe.name}",
            }
    finally:
        if got_lock:
            from loop.private_runtime import release_run_lock  # noqa: E402

            release_run_lock(lock_path)
    wall_ms = int((time.perf_counter() - t0) * 1000)
    log_text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log_path.write_text(log_text, encoding="utf-8")

    bmp = _find_bmp(_live_capture_hits(BMP_LEAF, min_mtime=t0_wall))
    mark_hits = _live_capture_hits(MARK_LEAF, min_mtime=t0_wall)
    mark = mark_hits[0] if mark_hits else None
    cap = _copy_into_row(row_id, bmp, mark, min_mtime=t0_wall)
    marks = cap["marks"]
    marks_set = set(marks)
    # Bare perf rows mark pointcloud-skip instead of pointcloud-ok.
    # Accept present-ok+bmp-ok when teardown misses the final "pass" mark
    # (same spirit as leftover accept_nonzero_rc_if_bmp).
    marks_ok = (
        "pointcloud-ok" in marks_set or "pointcloud-skip" in marks_set
    ) and (
        "pass" in marks_set
        or ("present-ok" in marks_set and "bmp-ok" in marks_set)
    )
    bmp_ok = bool(cap["bmp"]) and int(cap["bmp_bytes"] or 0) > 10000
    row_pass = marks_ok and (bmp_ok or row_id == "null")
    perf =     _load_perf_json(
        _fresh_file(
            _live_capture_hits(
                "plugin-showcase-world3d-perf.json", min_mtime=t0_wall
            )
            or [PLUGIN_CAP / "plugin-showcase-world3d-perf.json"],
            t0_wall,
        ),
        MATRIX / row_id,
    )
    note = (
        "china DEM; SMT_PLUGIN_WORLD3D_PERF_BARE strips sky/ocean/cloud/fog + "
        "pointcloud; prefer ms_per_present over wall_ms"
    )
    if row_id == "null":
        note = (
            "smoke full product path (not bare); "
            "prefer ms_per_present over wall_ms"
        )
    row = {
        "row_id": row_id,
        "engine": "plugin.world3d",
        "backend": backend,
        "parallel": parallel,
        "rc": proc.returncode,
        "wall_ms": wall_ms,
        "bmp": cap["bmp"],
        "bmp_bytes": cap["bmp_bytes"],
        "inspect_png": cap["inspect_png"],
        "marks": marks,
        "pass": row_pass,
        "log": str(log_path.relative_to(OUT)).replace("\\", "/"),
        "note": note,
    }
    row.update(perf)
    return row


def _fmt_num(v) -> str:
    if v is None:
        return "—"
    if isinstance(v, float):
        return f"{v:.3f}"
    return str(v)


def main(argv: list[str] | None = None) -> int:
    import argparse

    ap = argparse.ArgumentParser(
        description="world3d equal-profile backend/parallel matrix (bare peers)"
    )
    ap.add_argument(
        "--row",
        action="append",
        dest="rows",
        metavar="ROW_ID",
        help="Run only these row_ids (default: all). e.g. --row flycube",
    )
    args = ap.parse_args(argv)
    want = set(args.rows) if args.rows else None

    MATRIX.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    for row_id, backend, parallel, kind, role, overlay in ROWS:
        if want is not None and row_id not in want:
            continue
        print(f"world3d-matrix: run {row_id} backend={backend} "
            f"kind={kind} role={role}",
            flush=True,
        )
        row = run_views_world3d(row_id, backend, parallel, overlay)
        # First Views row can flake on DXGI/device race after kill;
        # one retry keeps the matrix usable for fair warm compare.
        if role in ("perf", "scenic") and not row.get("pass"):
            print(f"  retry {row_id} after flake", flush=True)
            row = run_views_world3d(row_id, backend, parallel, overlay)
        row["role"] = role
        if role == "smoke":
            note = row.get("note") or ""
            smoke_tag = "smoke-only (not a performance peer)"
            row["note"] = f"{note}; {smoke_tag}" if note else smoke_tag
        if role == "scenic":
            note = row.get("note") or ""
            tag = (
                "content-hosted scenic::Engine GDI (same china document; "
                "not a GPU peer vs FlyCube/Scenic GL/Scenic D3D11)"
            )
            row["note"] = f"{note}; {tag}" if note else tag
        rows.append(row)
        print(
            f"  rc={row.get('rc')} ms/p_warm={row.get('ms_per_present')} "
            f"cold={row.get('ms_per_present_cold')} "
            f"all={row.get('ms_per_present_all')} "
            f"wall_ms={row.get('wall_ms')} pass={row.get('pass')} "
            f"bmp_bytes={row.get('bmp_bytes')} role={role}",
            flush=True,
        )
    _kill_showcase_procs()

    csv_path = MATRIX / "world3d_backend_matrix.csv"
    json_path = MATRIX / "world3d_backend_matrix.json"
    fields = [
        "row_id", "role", "engine", "backend", "parallel", "rc",
        "ms_per_present", "ms_per_present_cold", "ms_per_present_all",
        "present_ms", "present_ms_warm", "present_count", "discard_cold",
        "warm_count", "mesh_ms", "sync_ms", "rebuild_ms", "record_ms",
        "present_swap_ms", "upload_ms", "pso_ms", "dem_load_ms", "tess_ms",
        "hypso_ms", "wall_ms", "bmp_bytes", "pass", "bmp",
        "inspect_png", "perf_json", "note",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(r)
    json_path.write_text(
        json.dumps({"equal_profile": "world3d_opt_matrix", "rows": rows}, indent=2),
        encoding="utf-8",
    )

    perf_rows = [r for r in rows if r.get("role") == "perf"]
    smoke_rows = [r for r in rows if r.get("role") == "smoke"]
    scenic_rows = [r for r in rows if r.get("role") == "scenic"]

    md_path = MATRIX / "MATRIX.md"
    lines = [
        "# world3d equal-profile matrix",
        "",
        "同等渲染物料及效果 · 并行策略×图像驱动（GL D3D FlyCube等）",
        "",
        "Env: `SMT_SCENE3D_ENGINE` + `SMT_STEREO_API` (scenic rhi3d GL/D3D).",
        "GDI omitted (not a 3D GPU peer). Null is smoke-only.",
        "",
        "**Primary metric: warm `ms_per_present`** (discard first cold frame).",
        "`ms/p_all` includes cold upload; `wall_ms` is process wall — do not rank by them.",
        "FlyCube bare = DEM-only 640x480 ×5; Scenic GL/D3D china stereo same count/discard.",
        "Cold attribution (FlyCube): `dem_load` / `tess` / `hypso` / `upload` / `pso` "
        "from `cold_phase` in `plugin-showcase-world3d-perf.json`.",
        "",
        "## Screenshots",
        "",
        "| Row | Role | Backend | Parallel | Inspect | pass |",
        "| --- | --- | --- | --- | --- | --- |",
    ]
    for r in rows:
        insp = r.get("inspect_png") or r.get("bmp") or "—"
        lines.append(
            f"| {r['row_id']} | {r.get('role')} | {r['backend']} | "
            f"{r['parallel']} | `{insp}` | {r.get('pass')} |"
        )
    lines += [
        "",
        "## Performance (warm ms_per_present)",
        "",
        "Perf peers only (FlyCube prep axes + Scenic/GL + Scenic/D3D11).",
        "",
        "| Row | Backend | Parallel | warm ms/p | cold | all | n | discard | "
        "mesh | record | swap | wall_ms* | pass |",
        "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | "
        "---: | ---: | ---: | --- |",
    ]
    for r in perf_rows:
        lines.append(
            f"| {r['row_id']} | {r['backend']} | {r['parallel']} | "
            f"{_fmt_num(r.get('ms_per_present'))} | "
            f"{_fmt_num(r.get('ms_per_present_cold'))} | "
            f"{_fmt_num(r.get('ms_per_present_all'))} | "
            f"{_fmt_num(r.get('present_count'))} | "
            f"{_fmt_num(r.get('discard_cold'))} | "
            f"{_fmt_num(r.get('mesh_ms'))} | "
            f"{_fmt_num(r.get('record_ms'))} | "
            f"{_fmt_num(r.get('present_swap_ms'))} | "
            f"{_fmt_num(r.get('wall_ms'))} | {r.get('pass')} |"
        )
    lines += [
        "",
        "## Cold attribution (first present)",
        "",
        "FlyCube rows expose seed/GPU cold phases; Scenic GL/D3D may leave these empty.",
        "",
        "| Row | cold ms/p | dem_load | tess | hypso | upload | pso | "
        "record* | mesh |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for r in perf_rows:
        lines.append(
            f"| {r['row_id']} | "
            f"{_fmt_num(r.get('ms_per_present_cold'))} | "
            f"{_fmt_num(r.get('dem_load_ms'))} | "
            f"{_fmt_num(r.get('tess_ms'))} | "
            f"{_fmt_num(r.get('hypso_ms'))} | "
            f"{_fmt_num(r.get('upload_ms'))} | "
            f"{_fmt_num(r.get('pso_ms'))} | "
            f"{_fmt_num((r.get('cold_phase') or {}).get('record_ms') if isinstance(r.get('cold_phase'), dict) else None)} | "
            f"{_fmt_num((r.get('cold_phase') or {}).get('mesh_ms') if isinstance(r.get('cold_phase'), dict) else r.get('mesh_ms'))} |"
        )
    lines += [
        "",
        "\\* cold `record` is exclusive of nested `upload`+`pso` when available.",
        "\\* `wall_ms` = whole-process wall; not comparable across engines.",
        "",
    ]
    if smoke_rows:
        lines += [
            "## Smoke (not performance)",
            "",
            "| Row | Backend | Parallel | wall_ms | pass | note |",
            "| --- | --- | --- | ---: | --- | --- |",
        ]
        for r in smoke_rows:
            lines.append(
                f"| {r['row_id']} | {r['backend']} | {r['parallel']} | "
                f"{_fmt_num(r.get('wall_ms'))} | {r.get('pass')} | "
                f"{r.get('note', '')} |"
            )
        lines.append("")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"wrote {csv_path}")
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    failed = [r for r in rows if not r.get("pass")]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
