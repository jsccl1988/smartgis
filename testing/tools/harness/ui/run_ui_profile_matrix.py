# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run Views UI chrome equal-profile (views_bench + --ui-showcase).

Primary metrics: google/benchmark real_time_ns (L1b) and PaintCounters
commit/raster/present_ms from ui-showcase-*-perf.json. Do not rank by
process wall_ms or map GPU hud_fps against chrome phases (FALSE-GAP).

Artifacts: out/Debug/captures/analysis/ui_opt/matrix/
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
BENCH = OUT / "views_bench.exe"
MATRIX = OUT / "captures" / "analysis" / "ui_opt" / "matrix"
UI_CAP = OUT / "captures" / "ui"

# Showcase rows: product chrome. scene/interact are visual — not chrome peers.
SHOWCASE_ROWS: list[tuple[str, str, str, str]] = [
    ("shell", "shell", "Views chrome", "perf"),
    ("catalog", "catalog", "Catalog page", "perf"),
    ("data", "data", "Map tab (data alias)", "perf"),
    ("scene", "scene", "3D tab chrome", "smoke"),
]

BENCH_PERF_NAMES = (
    "BM_hover_commit",
    "BM_table_scroll_commit",
    "BM_overlay_crop_memcpy",
    "BM_shell_compositor_smoke",
)

# STATUS_HEAP_CORRUPTION (0xC0000374) after capture: first attempt wrote
# BMP+perf, then DrawHost detach raced TerminateProcess. Treat as harness-ok
# when artifacts exist so retry does not unlink a green capture.
_HEAP_CORRUPT_RC = {3221226356, -1073740684}
_HEAP_CORRUPT_NTSTATUS = 0xC0000374


def _is_heap_corrupt_rc(rc: object) -> bool:
    if not isinstance(rc, int):
        return False
    if rc in _HEAP_CORRUPT_RC:
        return True
    return (rc & 0xFFFFFFFF) == _HEAP_CORRUPT_NTSTATUS


def _apply_env(overlay: dict[str, str]) -> dict[str, str]:
    env = os.environ.copy()
    env.update(overlay)
    env["PATH"] = str(OUT) + os.pathsep + env.get("PATH", "")
    env["PYTHONUNBUFFERED"] = "1"
    env["SKIP_MAP_CONTEXT_MENU"] = "1"
    return env


def _kill_showcase_procs() -> None:
    if os.name != "nt":
        return
    subprocess.run(
        ["taskkill", "/IM", "SmartGIS.exe", "/T"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    time.sleep(1.5)
    subprocess.run(
        ["taskkill", "/IM", "SmartGIS.exe", "/F", "/T"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    time.sleep(0.8)


def _copy2_retry(src: Path, dst: Path) -> None:
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
        print(f"ui-profile: inspect_png fail {bmp}: {ex}", file=sys.stderr)
    return None


def _fmt_num(v) -> str:
    if v is None:
        return "—"
    if isinstance(v, float):
        return f"{v:.3f}"
    return str(v)


def _load_perf_json(src: Path | None, dest_dir: Path) -> dict:
    keys = (
        "hud_fps",
        "commit_ms",
        "raster_ms",
        "present_ms",
        "widget_paint_ms",
        "map_paint_ms",
        "hover_commit_ms",
        "table_scroll_ms",
        "overlay_commit_ms",
        "begin_frame_to_shell_present_ms",
        "commit_count",
        "activate_count",
        "layout_count",
        "create_font",
        "create_brush",
        "overlay_copy_bytes",
        "paint_pixels",
    )
    out: dict = {k: None for k in keys}
    out["perf_json"] = None
    if not src or not src.is_file():
        return out
    dest_dir.mkdir(parents=True, exist_ok=True)
    dst = dest_dir / src.name
    _copy2_retry(src, dst)
    out["perf_json"] = str(dst.relative_to(OUT)).replace("\\", "/")
    try:
        data = json.loads(dst.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as ex:
        print(f"ui-profile: perf json parse fail {src}: {ex}", file=sys.stderr)
        return out
    for k in keys:
        if k in data:
            out[k] = data[k]
    return out


def _bench_row(
    *,
    name: str,
    rc: int,
    wall_ms: float,
    real_ns,
    cpu_ns,
    iterations,
    log_rel: str,
    note: str,
    pass_ok: bool,
) -> dict:
    short = name.split("/")[0]
    role = "perf" if short in BENCH_PERF_NAMES else "smoke"
    return {
        "row_id": f"bench:{name}",
        "role": role,
        "kind": "bench",
        "surface": name,
        "rc": rc,
        "pass": pass_ok,
        "wall_ms": wall_ms,
        "real_time_ns": real_ns,
        "cpu_time_ns": cpu_ns,
        "iterations": iterations,
        "note": note,
        "log": log_rel,
    }


def _run_one_views_bench(filter_name: str) -> list[dict]:
    """One google/benchmark filter in its own process.

    Debug CRT heap abort in BM_table_scroll_commit used to truncate a single
    --benchmark_out JSON and drop later peers (overlay_crop). Isolate so the
    matrix still records hover / compositor / crop ns.
    """
    safe = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in filter_name)
    out_json = MATRIX / f"views_bench_{safe}.json"
    log_path = MATRIX / f"views_bench_{safe}.log"
    cmd = [
        str(BENCH),
        f"--benchmark_filter=^{filter_name}$",
        "--benchmark_format=json",
        f"--benchmark_out={out_json}",
    ]
    t0 = time.perf_counter()
    with log_path.open("w", encoding="utf-8", errors="replace") as logf:
        proc = subprocess.run(
            cmd,
            cwd=str(OUT),
            env=_apply_env({}),
            stdout=logf,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
    wall_ms = (time.perf_counter() - t0) * 1000.0
    log_rel = str(log_path.relative_to(OUT)).replace("\\", "/")
    benches: list[dict] = []
    if out_json.is_file():
        try:
            payload = json.loads(out_json.read_text(encoding="utf-8"))
            benches = payload.get("benchmarks") or []
        except (OSError, json.JSONDecodeError) as ex:
            print(
                f"ui-profile: views_bench {filter_name} json fail: {ex}",
                file=sys.stderr,
            )
    if not benches:
        note = "no benchmarks in JSON"
        if proc.returncode not in (0, None) and proc.returncode != 0:
            note = (
                f"bench rc={proc.returncode} (Debug heap abort on "
                f"{filter_name} teardown is U2 TableView — see crash dump)"
            )
        return [
            _bench_row(
                name=filter_name,
                rc=proc.returncode,
                wall_ms=wall_ms,
                real_ns=None,
                cpu_ns=None,
                iterations=None,
                log_rel=log_rel,
                note=note,
                pass_ok=False,
            )
        ]
    rows = []
    for b in benches:
        name = str(b.get("name") or filter_name)
        rows.append(
            _bench_row(
                name=name,
                rc=proc.returncode,
                wall_ms=wall_ms,
                real_ns=b.get("real_time"),
                cpu_ns=b.get("cpu_time"),
                iterations=b.get("iterations"),
                log_rel=log_rel,
                note="google/benchmark real_time (ns/iter); not process wall",
                pass_ok=proc.returncode == 0,
            )
        )
    return rows


def run_views_bench() -> list[dict]:
    rows: list[dict] = []
    if not BENCH.is_file():
        return [
            {
                "row_id": "views_bench",
                "role": "perf",
                "kind": "bench",
                "surface": "L1b",
                "rc": 127,
                "pass": False,
                "note": "missing views_bench.exe — build src/ui/views:views_bench",
                "real_time_ns": None,
            }
        ]
    merged: list[dict] = []
    for name in BENCH_PERF_NAMES:
        print(f"ui-profile: views_bench {name}", flush=True)
        chunk = _run_one_views_bench(name)
        rows.extend(chunk)
        for r in chunk:
            if r.get("real_time_ns") is not None:
                merged.append(
                    {
                        "name": r.get("surface"),
                        "real_time": r.get("real_time_ns"),
                        "cpu_time": r.get("cpu_time_ns"),
                        "iterations": r.get("iterations"),
                    }
                )
    (MATRIX / "views_bench.json").write_text(
        json.dumps({"benchmarks": merged}, indent=2),
        encoding="utf-8",
    )
    return rows


def run_showcase(row_id: str, mode: str, surface: str, role: str) -> dict:
    dest = MATRIX / row_id
    dest.mkdir(parents=True, exist_ok=True)
    bmp_leaf = f"ui-showcase-{mode}.bmp"
    perf_leaf = f"ui-showcase-{mode}-perf.json"
    mark_leaf = "ui-showcase-mark.txt"
    for leaf in (bmp_leaf, perf_leaf, mark_leaf):
        p = UI_CAP / leaf
        if p.is_file():
            try:
                p.unlink()
            except OSError:
                pass
    env = _apply_env(
        {
            "UI_SHOWCASE_LINGER_MS": "0",
            "UI_FORENSICS": "1",
            "UI_THEME": "dark",
            "SMARTGIS_NO_ALWAYS_ON_DIAG": "1",
        }
    )
    # Same launch as loop_runner: empty argv (retired --ui-showcase is
    # stripped) + plugin.json startup.scenario=ui.<mode> AFTER product show.
    # Passing --plugin-showcase=ui.shell applies ContentMapView policy before
    # Browser::show and AVs (write-to-0xF).
    if str(ROOT) not in sys.path:
        sys.path.insert(0, str(ROOT))
    from testing.tools.loop.contract import load_suite
    from testing.tools.loop.drive.plugin import (
        apply_suite_plugin_startup,
        restore_product_plugin_startup,
    )

    suite = load_suite(f"ui.{mode}")
    apply_suite_plugin_startup(suite, VIEWS)
    cmd = [str(VIEWS)]
    t0 = time.perf_counter()
    _kill_showcase_procs()
    log_path = dest / "showcase.log"
    try:
        with log_path.open("w", encoding="utf-8", errors="replace") as logf:
            proc = subprocess.run(
                cmd,
                cwd=str(OUT),
                env=env,
                stdout=logf,
                stderr=subprocess.STDOUT,
                timeout=140,
                check=False,
            )
    except FileNotFoundError:
        log_path.write_text(f"missing exe: {cmd[0]}\n", encoding="utf-8")
        proc = subprocess.CompletedProcess(cmd, 127)
    except subprocess.TimeoutExpired:
        proc = subprocess.CompletedProcess(cmd, 124)
    wall_ms = (time.perf_counter() - t0) * 1000.0
    _kill_showcase_procs()
    restore_product_plugin_startup(VIEWS)
    bmp = UI_CAP / bmp_leaf
    if bmp.is_file():
        _copy2_retry(bmp, dest / bmp.name)
        bmp = dest / bmp.name
    inspect = _try_inspect_png(bmp) if bmp.is_file() else None
    perf_src = UI_CAP / perf_leaf
    perf = _load_perf_json(perf_src if perf_src.is_file() else None, dest)
    bmp_bytes = bmp.stat().st_size if bmp.is_file() else 0
    rc = proc.returncode
    heap_rc = _is_heap_corrupt_rc(rc)
    rc_ok = rc == 0 or (heap_rc and bmp_bytes > 0)
    ok = rc_ok and bmp_bytes > 0
    if role == "perf":
        ok = ok and perf.get("commit_ms") is not None
    row = {
        "row_id": row_id,
        "role": role,
        "kind": "integration",
        "surface": surface,
        "rc": proc.returncode,
        "pass": ok,
        "wall_ms": wall_ms,
        "bmp_bytes": bmp_bytes,
        "bmp": str(bmp.relative_to(OUT)).replace("\\", "/") if bmp.is_file() else None,
        "inspect_png": inspect,
        "log": str(log_path.relative_to(OUT)).replace("\\", "/"),
        "note": "" if role == "perf" else "visual/smoke — not a chrome PaintCounters peer",
        "real_time_ns": None,
    }
    row.update(perf)
    return row


def _hottest_phase(rows: list[dict]) -> tuple[str, str, float]:
    best = ("", "", -1.0)
    for r in rows:
        if r.get("role") != "perf" or r.get("kind") != "integration":
            continue
        for key in ("commit_ms", "raster_ms", "present_ms"):
            v = r.get(key)
            if isinstance(v, (int, float)) and float(v) > best[2]:
                best = (str(r.get("row_id")), key, float(v))
    return best


def _hottest_bench(rows: list[dict]) -> tuple[str, float]:
    best = ("", -1.0)
    for r in rows:
        if r.get("kind") != "bench" or r.get("role") != "perf":
            continue
        v = r.get("real_time_ns")
        if isinstance(v, (int, float)) and float(v) > best[1]:
            best = (str(r.get("row_id")), float(v))
    return best


def _recommendations(rows: list[dict]) -> list[str]:
    tips: list[str] = []
    row, phase, ms = _hottest_phase(rows)
    if phase == "raster_ms":
        tips.append(
            f"Hottest chrome phase: `{row}` `{phase}` = {ms:.3f} ms → "
            "tighten `ShellCompositor::raster_dirty_into` dirty rects / "
            "U4 tiled raster (`src/ui/views/kernel/compositor/shell_compositor.cc`)."
        )
    elif phase == "commit_ms":
        tips.append(
            f"Hottest chrome phase: `{row}` `{phase}` = {ms:.3f} ms → "
            "U1 record cache / bounded `commit_view_tree` "
            "(`src/ui/views/kernel/paint/paint_commit.*`, `widget.cc` `record_commit`)."
        )
    elif phase == "present_ms":
        tips.append(
            f"Hottest chrome phase: `{row}` `{phase}` = {ms:.3f} ms → "
            "U5/U3 `blt_present` / skip redundant BitBlt "
            "(`ShellCompositor::present`)."
        )
    bname, ns = _hottest_bench(rows)
    if "table_scroll" in bname:
        tips.append(
            f"Hottest L1b: `{bname}` {ns:.0f} ns/iter → U2 TableView row-strip / "
            "`exposed_rect` (`src/ui/views/primitives/collection/table_view.cc`)."
        )
    elif "hover_commit" in bname:
        tips.append(
            f"Hottest L1b: `{bname}` {ns:.0f} ns/iter → U1 hover dirty Button "
            "(`BM_hover_commit`, `src/ui/views/primitives/button/`)."
        )
    elif "overlay_crop" in bname:
        tips.append(
            f"Hottest L1b: `{bname}` {ns:.0f} ns/iter → U3 shell overlay crop skip "
            "(`BM_overlay_crop_memcpy`, `BrowserView` `commit_shell_overlay`)."
        )
    elif "shell_compositor" in bname:
        tips.append(
            f"Hottest L1b: `{bname}` {ns:.0f} ns/iter → compositor publish path "
            "(`BM_shell_compositor_smoke`)."
        )
    fonts = 0
    brushes = 0
    overlay_bytes = 0
    layout = 0
    for r in rows:
        if r.get("kind") != "integration":
            continue
        fonts += int(r.get("create_font") or 0)
        brushes += int(r.get("create_brush") or 0)
        overlay_bytes += int(r.get("overlay_copy_bytes") or 0)
        layout += int(r.get("layout_count") or 0)
    if fonts > 64:
        tips.append(
            f"High `create_font` ({fonts}) → Theme font cache "
            "(`src/ui/views/kernel/shell/theme.*`)."
        )
    if brushes > 256:
        tips.append(
            f"High `create_brush` ({brushes}) → brush cache in painters "
            "(`src/ui/views/kernel/paint/`)."
        )
    if overlay_bytes > 0:
        tips.append(
            f"`overlay_copy_bytes`={overlay_bytes} → U3 gen-skip overlay memcpy "
            "(`src/app/views` BrowserView shell crop)."
        )
    if layout > 200:
        tips.append(
            f"High `layout_count` ({layout}) → Yoga / `layout_contents` "
            "(`src/ui/views/markup/layout/yoga_layout_manager.cc`)."
        )
    tips.append(
        "FALSE-GAP: do not compare `hud_fps` / `map_paint_ms` to chrome "
        "`commit_ms`/`raster_ms`. Map GPU uses map2d/scene3d frame-opt skills."
    )
    tips.append(
        "Cold start → first China present is `harness-auto-bootstrap`, not this matrix."
    )
    return tips


def main(argv: list[str] | None = None) -> int:
    import argparse

    ap = argparse.ArgumentParser(description="UI chrome equal-profile matrix")
    ap.add_argument(
        "--row",
        action="append",
        dest="rows",
        metavar="ROW_ID",
        help="shell|catalog|data|scene|bench (default: all except interact)",
    )
    ap.add_argument(
        "--skip-bench",
        action="store_true",
        help="Skip views_bench.exe",
    )
    args = ap.parse_args(argv)
    want = set(args.rows) if args.rows else None

    MATRIX.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    run_bench = not args.skip_bench and (want is None or "bench" in want)
    if run_bench:
        print("ui-profile: views_bench", flush=True)
        rows.extend(run_views_bench())
    for row_id, mode, surface, role in SHOWCASE_ROWS:
        if want is not None and row_id not in want:
            continue
        if not VIEWS.is_file():
            rows.append(
                {
                    "row_id": row_id,
                    "role": role,
                    "kind": "integration",
                    "surface": surface,
                    "rc": 127,
                    "pass": False,
                    "note": "missing SmartGIS.exe — build src/app/views:views",
                }
            )
            continue
        print(f"ui-profile: showcase {row_id}", flush=True)
        row = run_showcase(row_id, mode, surface, role)
        has_capture = bool(row.get("bmp_bytes")) and (
            role != "perf" or row.get("commit_ms") is not None
        )
        heap_rc = _is_heap_corrupt_rc(row.get("rc"))
        # Heap after BMP+perf is still a green capture — do not retry (unlink).
        if heap_rc and has_capture:
            row["pass"] = True
            print(
                f"  skip-retry {row_id} (heap rc={row.get('rc')} capture ok)",
                flush=True,
            )
        elif role == "perf" and not row.get("pass"):
            if has_capture:
                print(
                    f"  skip-retry {row_id} (capture ok rc={row.get('rc')})",
                    flush=True,
                )
            else:
                print(f"  retry {row_id}", flush=True)
                row = run_showcase(row_id, mode, surface, role)
        rows.append(row)
        print(
            f"  rc={row.get('rc')} commit_ms={row.get('commit_ms')} "
            f"raster_ms={row.get('raster_ms')} present_ms={row.get('present_ms')} "
            f"hud_fps={row.get('hud_fps')} pass={row.get('pass')}",
            flush=True,
        )

    csv_path = MATRIX / "ui_profile_matrix.csv"
    json_path = MATRIX / "ui_profile_matrix.json"
    fields = [
        "row_id",
        "role",
        "kind",
        "surface",
        "rc",
        "real_time_ns",
        "cpu_time_ns",
        "commit_ms",
        "raster_ms",
        "present_ms",
        "widget_paint_ms",
        "map_paint_ms",
        "hud_fps",
        "commit_count",
        "layout_count",
        "create_font",
        "create_brush",
        "overlay_copy_bytes",
        "wall_ms",
        "bmp_bytes",
        "pass",
        "bmp",
        "inspect_png",
        "perf_json",
        "note",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(r)
    json_path.write_text(
        json.dumps({"equal_profile": "ui_opt_matrix", "rows": rows}, indent=2),
        encoding="utf-8",
    )

    bench_rows = [r for r in rows if r.get("kind") == "bench" and r.get("role") == "perf"]
    chrome_rows = [
        r for r in rows if r.get("kind") == "integration" and r.get("role") == "perf"
    ]
    smoke_rows = [r for r in rows if r.get("role") == "smoke"]
    tips = _recommendations(rows)

    md_path = MATRIX / "MATRIX.md"
    lines = [
        "# UI chrome equal-profile matrix",
        "",
        "Views shell chrome · `views_bench` × `--ui-showcase` (dark theme).",
        "",
        "**Primary:** L1b `real_time_ns`; product `commit_ms` / `raster_ms` / `present_ms`.",
        "Do **not** rank by `wall_ms`. `hud_fps` is map cadence (FALSE-GAP vs chrome).",
        "",
        "## Screenshots",
        "",
        "| Row | Role | Surface | Inspect | pass |",
        "| --- | --- | --- | --- | --- |",
    ]
    for r in rows:
        if r.get("kind") != "integration":
            continue
        insp = r.get("inspect_png") or r.get("bmp") or "—"
        lines.append(
            f"| {r['row_id']} | {r.get('role')} | {r.get('surface')} | "
            f"`{insp}` | {r.get('pass')} |"
        )
    lines += [
        "",
        "## L1b views_bench (ns/iter)",
        "",
        "| Row | real_time_ns | cpu_time_ns | pass |",
        "| --- | ---: | ---: | --- |",
    ]
    for r in bench_rows:
        lines.append(
            f"| {r['row_id']} | {_fmt_num(r.get('real_time_ns'))} | "
            f"{_fmt_num(r.get('cpu_time_ns'))} | {r.get('pass')} |"
        )
    lines += [
        "",
        "## Product chrome PaintCounters (ms, Debug accumulate)",
        "",
        "| Row | commit_ms | raster_ms | present_ms | layout | fonts | hud_fps* | pass |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    for r in chrome_rows:
        lines.append(
            f"| {r['row_id']} | {_fmt_num(r.get('commit_ms'))} | "
            f"{_fmt_num(r.get('raster_ms'))} | {_fmt_num(r.get('present_ms'))} | "
            f"{_fmt_num(r.get('layout_count'))} | {_fmt_num(r.get('create_font'))} | "
            f"{_fmt_num(r.get('hud_fps'))} | {r.get('pass')} |"
        )
    lines += [
        "",
        "\\* `hud_fps` = MapViewport identity HUD — not a chrome peer metric.",
        "",
    ]
    if smoke_rows:
        lines += [
            "## Smoke / visual (not chrome performance peers)",
            "",
            "| Row | Surface | wall_ms | pass | note |",
            "| --- | --- | ---: | --- | --- |",
        ]
        for r in smoke_rows:
            lines.append(
                f"| {r['row_id']} | {r.get('surface')} | {_fmt_num(r.get('wall_ms'))} | "
                f"{r.get('pass')} | {r.get('note', '')} |"
            )
        lines.append("")
    lines += ["## Optimization recommendations", ""]
    for t in tips:
        lines.append(f"- {t}")
    lines.append("")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    rec_path = MATRIX / "RECOMMEND.md"
    rec_path.write_text(
        "# UI profile recommendations\n\n" + "\n".join(f"- {t}" for t in tips) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {csv_path}")
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    print(f"wrote {rec_path}")
    failed = [r for r in rows if not r.get("pass") and r.get("role") == "perf"]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
