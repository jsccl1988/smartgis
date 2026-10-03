# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run plugin.world3d equal-profile backend/parallel matrix.

Same True-Earth Scene3D materials on FlyCube rows (SmartGisViews
--plugin-showcase=world3d). Leftover Stereo/GL + D3D11 rows use SmartGis
--scene3d-showcase china with SMT_STEREO_API / SMT_SCENE3D_ENGINE (real BMPs;
not fabricated). Note: leftover IR seed is china DEM stereo — phase-fair vs
FlyCube product, but not identical overlay (pointcloud) work.

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
VIEWS = OUT / "SmartGisViews.exe"
LEGACY = OUT / "SmartGis.exe"
MATRIX = OUT / "captures" / "analysis" / "world3d_opt" / "matrix"
PLUGIN_CAP = OUT / "captures" / "plugin"
LEGACY_CAP = OUT / "captures" / "legacy"
BMP_LEAF = "plugin-showcase-world3d.bmp"
MARK_LEAF = "plugin-showcase-mark.txt"

# row_id, backend_label, parallel_label, kind, env overlays
# kind: views_world3d | legacy_stereo
ROWS: list[tuple[str, str, str, str, dict[str, str | None]]] = [
    # Views world3d: leave SMT_SCENE3D_ENGINE unset so plugin-showcase keeps
    # the GDI shell default; FlyCube is acquired on the showcase HWND only.
    (
        "flycube",
        "FlyCube/DX12",
        "prep_default",
        "views_world3d",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "1",
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_GPUSCENE_PREP_PARALLEL": None,
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "prep_par_off",
        "FlyCube/DX12",
        "prep_0",
        "views_world3d",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "1",
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_GPUSCENE_PREP_PARALLEL": "0",
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "prep_par_on",
        "FlyCube/DX12",
        "prep_on",
        "views_world3d",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "1",
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_GPUSCENE_PREP_PARALLEL": "1",
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "null",
        "Null",
        "gpu_off",
        "views_world3d",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "0",
            "SMT_PREFER_GDI_DEVICE": None,
            "SMT_GPUSCENE_PREP_PARALLEL": None,
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "gdi",
        "GDI",
        "prefer_gdi",
        "views_world3d",
        {
            "SMT_PLUGIN_WORLD3D_GPU": "1",
            "SMT_PREFER_GDI_DEVICE": "1",
            "SMT_GPUSCENE_PREP_PARALLEL": None,
            "SMT_SCENE3D_ENGINE": None,
        },
    ),
    (
        "gl",
        "Stereo/GL",
        "leftover_serial",
        "legacy_stereo",
        {
            # SmartGis does not use SMT_SCENE3D_ENGINE; only stereo API.
            "SMT_SCENE3D_ENGINE": None,
            "SMT_STEREO_API": "OpenGL",
            "SMT_SCENE3D_SHOWCASE_D3D": "0",
            "SMT_SCENE3D_SHOWCASE_LINGER_MS": "0",
            "SMT_RHI3D_FRAME_JOB": "0",
            "SMT_RHI3D_PREP_PARALLEL": "0",
        },
    ),
    (
        "d3d_leftover",
        "Leftover/D3D11",
        "leftover_serial",
        "legacy_stereo",
        {
            "SMT_SCENE3D_ENGINE": None,
            "SMT_STEREO_API": "Direct3D",
            "SMT_SCENE3D_SHOWCASE_D3D": "1",
            "SMT_SCENE3D_SHOWCASE_LINGER_MS": "0",
            "SMT_RHI3D_FRAME_JOB": "0",
            "SMT_RHI3D_PREP_PARALLEL": "0",
            "SMT_RHI3D_D3D_DEFERRED": "0",
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
    """Drop leftover PE locks / DXGI adapters between matrix rows."""
    if os.name != "nt":
        return
    for name in ("SmartGisViews.exe", "SmartGis.exe"):
        subprocess.run(
            ["taskkill", "/IM", name, "/T"],
            capture_output=True,
            text=True,
            check=False,
        )
    time.sleep(2.0)
    for name in ("SmartGisViews.exe", "SmartGis.exe"):
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
        shutil.copy2(src_bmp, dst)
        out["bmp"] = str(dst.relative_to(OUT)).replace("\\", "/")
        out["bmp_bytes"] = dst.stat().st_size
        out["inspect_png"] = _try_inspect_png(dst)
    if src_mark and src_mark.is_file():
        dst_m = dest_dir / src_mark.name
        shutil.copy2(src_mark, dst_m)
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
    env = _apply_env(os.environ.copy(), overlay)

    t0 = time.perf_counter()
    t0_wall = time.time()
    proc = subprocess.run(
        [str(VIEWS), "--plugin-showcase=world3d"],
        cwd=str(OUT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=240,
    )
    wall_ms = int((time.perf_counter() - t0) * 1000)
    log_text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log_path.write_text(log_text, encoding="utf-8")

    bmp = _find_bmp(
        [PLUGIN_CAP / BMP_LEAF]
        + list((OUT / "captures").rglob(BMP_LEAF))
    )
    mark = PLUGIN_CAP / MARK_LEAF
    if not mark.is_file():
        for p in (OUT / "captures").rglob(MARK_LEAF):
            mark = p
            break
    cap = _copy_into_row(
        row_id, bmp, mark if mark.is_file() else None, min_mtime=t0_wall
    )
    marks = cap["marks"]
    required = {"pointcloud-ok", "pass"}
    marks_ok = required.issubset(set(marks))
    bmp_ok = bool(cap["bmp"]) and int(cap["bmp_bytes"] or 0) > 10000
    row_pass = proc.returncode == 0 and marks_ok and (
        bmp_ok or row_id == "null"
    )
    return {
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
        "note": "equal materials: world3d seed + pointcloud; wall_ms=process wall",
    }


def run_legacy_stereo(row_id: str, backend: str, parallel: str,
                      overlay: dict[str, str | None]) -> dict:
    if not LEGACY.is_file():
        return {
            "row_id": row_id,
            "engine": "legacy.scene3d",
            "backend": backend,
            "parallel": parallel,
            "rc": None,
            "wall_ms": None,
            "bmp": None,
            "bmp_bytes": 0,
            "inspect_png": None,
            "marks": [],
            "pass": False,
            "log": None,
            "note": f"missing {LEGACY.name}; build SmartGis",
        }

    MATRIX.mkdir(parents=True, exist_ok=True)
    log_path = MATRIX / f"{row_id}.log"
    _kill_showcase_procs()
    env = _apply_env(os.environ.copy(), overlay)

    t0 = time.perf_counter()
    t0_wall = time.time()
    proc = subprocess.run(
        [str(LEGACY), "--scene3d-showcase", "china"],
        cwd=str(OUT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=180,
    )
    wall_ms = int((time.perf_counter() - t0) * 1000)
    log_text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log_path.write_text(log_text, encoding="utf-8")

    is_d3d = row_id == "d3d_leftover"
    # Prefer backend-specific leaf so GL/D3D do not overwrite each other.
    leaf_pref = (
        "legacy-scene3d-showcase-china-d3d.bmp"
        if is_d3d
        else "legacy-scene3d-showcase-china-gl.bmp"
    )
    leaf_alt = "legacy-scene3d-showcase-china.bmp"
    candidates = [
        LEGACY_CAP / leaf_pref,
        OUT / leaf_pref,
        LEGACY_CAP / leaf_alt,
        OUT / leaf_alt,
    ]
    for p in (OUT / "captures").rglob("legacy-scene3d-showcase-china*.bmp"):
        candidates.append(p)
    bmp = _find_bmp(candidates)

    mark_leaf = "legacy-scene3d-showcase-mark.txt"
    mark = LEGACY_CAP / mark_leaf
    if not mark.is_file():
        for p in (OUT / "captures").rglob(mark_leaf):
            mark = p
            break
        if not mark.is_file() and (OUT / mark_leaf).is_file():
            mark = OUT / mark_leaf

    cap = _copy_into_row(
        row_id, bmp, mark if mark.is_file() else None, min_mtime=t0_wall
    )
    bmp_ok = bool(cap["bmp"]) and int(cap["bmp_bytes"] or 0) > 10000
    # Match suite accept_nonzero_rc_if_bmp — leftover often AVs on teardown
    # after a valid HWND capture (heap / GL destroy).
    row_pass = bmp_ok
    note = (
        "leftover stereo china DEM via SMT_STEREO_API; "
        "not identical to world3d pointcloud overlay — fair phase/wall compare only"
    )
    if bmp_ok and proc.returncode not in (0, None):
        note += f"; rc={proc.returncode} (bmp accepted)"
    return {
        "row_id": row_id,
        "engine": "legacy.scene3d",
        "backend": backend,
        "parallel": parallel,
        "rc": proc.returncode,
        "wall_ms": wall_ms,
        "bmp": cap["bmp"],
        "bmp_bytes": cap["bmp_bytes"],
        "inspect_png": cap["inspect_png"],
        "marks": cap["marks"],
        "pass": row_pass,
        "log": str(log_path.relative_to(OUT)).replace("\\", "/"),
        "note": note,
    }


def main() -> int:
    MATRIX.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    for row_id, backend, parallel, kind, overlay in ROWS:
        print(
            f"world3d-matrix: run {row_id} backend={backend} kind={kind}",
            flush=True,
        )
        if kind == "legacy_stereo":
            row = run_legacy_stereo(row_id, backend, parallel, overlay)
        else:
            row = run_views_world3d(row_id, backend, parallel, overlay)
        rows.append(row)
        print(
            f"  rc={row.get('rc')} wall_ms={row.get('wall_ms')} "
            f"pass={row.get('pass')} bmp_bytes={row.get('bmp_bytes')}",
            flush=True,
        )
    _kill_showcase_procs()

    csv_path = MATRIX / "world3d_backend_matrix.csv"
    json_path = MATRIX / "world3d_backend_matrix.json"
    fields = [
        "row_id", "engine", "backend", "parallel", "rc", "wall_ms", "bmp_bytes",
        "pass", "bmp", "inspect_png", "note",
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

    md_path = MATRIX / "MATRIX.md"
    lines = [
        "# world3d equal-profile matrix",
        "",
        "同等渲染物料及效果 · 并行策略×图像驱动（GL D3D FlyCube等）",
        "",
        "Env: `SMT_SCENE3D_ENGINE` + `SMT_STEREO_API` (leftover GL/D3D).",
        "",
        "## Screenshots",
        "",
        "| Row | Backend | Parallel | Inspect | pass |",
        "| --- | --- | --- | --- | --- |",
    ]
    for r in rows:
        insp = r.get("inspect_png") or r.get("bmp") or "—"
        lines.append(
            f"| {r['row_id']} | {r['backend']} | {r['parallel']} | "
            f"`{insp}` | {r.get('pass')} |"
        )
    lines += [
        "",
        "## Performance (process wall)",
        "",
        "| Row | Backend | Parallel | wall_ms | bmp_bytes | pass | note |",
        "| --- | --- | --- | ---: | ---: | --- | --- |",
    ]
    for r in rows:
        wall = r.get("wall_ms")
        wall_s = "N/A" if wall is None else str(wall)
        lines.append(
            f"| {r['row_id']} | {r['backend']} | {r['parallel']} | "
            f"{wall_s} | {r.get('bmp_bytes') or 0} | {r.get('pass')} | "
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
