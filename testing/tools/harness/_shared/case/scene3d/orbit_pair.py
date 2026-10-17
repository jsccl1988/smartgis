# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Product Scene3D far/near orbit pair.

Two SmartGIS.exe launches, camera locked by SCENE3D_ORBIT_DISTANCE /
SCENE3D_ORBIT_PITCH (switch scene3d-orbit-distance / scene3d-orbit-pitch).
Each shot must pass plugin_scene3d (land visible, not a navy clear).

    py -3 testing/tools/harness/_shared/case/scene3d/orbit_pair.py
    py -3 testing/tools/harness/_shared/case/scene3d/orbit_pair.py --out Debug
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[4]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.drive.process import kill_exe, pe_launch_cwd  # noqa: E402
from loop.record.hwnd import (  # noqa: E402
    bring_hwnd_to_front,
    capture_flycube_present_bmp,
    capture_hwnd_bmp_ex,
    drop_topmost,
    find_top_level_hwnd_for_pid,
    find_window_by_title_substr,
)
from loop.score.bmp import score_bmp  # noqa: E402

# parents: scene3d -> case -> _shared -> harness -> tools. Repo is tools/../..
_REPO = _TOOLS.parent.parent


def _visible(score: dict) -> bool:
    land = float(score.get("landish_frac") or 0.0)
    navy = float(score.get("navy_clear_frac") or 1.0)
    return bool(score.get("ok")) or (land >= 0.08 and navy < 0.85)


def _shot(
    *,
    exe: Path,
    cap: Path,
    log_dir: Path,
    tag: str,
    distance: float,
    pitch: float,
) -> dict:
    kill_exe("SmartGIS.exe")
    err_path = log_dir / f"orbit_{tag}_stderr.txt"
    err_path.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["VIEWS_START_MAP_TAB"] = "scene3d"
    env["SYNC_CHINA_SEED"] = "1"
    env["SCENE3D_ORBIT_DISTANCE"] = f"{distance:.4f}"
    env["SCENE3D_ORBIT_PITCH"] = f"{pitch:.4f}"
    env["SMARTGIS_NO_ALWAYS_ON_DIAG"] = "1"
    cwd, run_env = pe_launch_cwd(exe, env)
    err = open(err_path, "w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(
        [str(exe)],
        cwd=str(cwd),
        env=run_env,
        stdout=subprocess.DEVNULL,
        stderr=err,
    )
    row: dict = {"tag": tag, "distance": distance, "pitch": pitch, "pid": proc.pid}
    try:
        hwnd, title = find_top_level_hwnd_for_pid(
            proc.pid, timeout_sec=90.0, min_area=50_000, title_substr="SmartGIS"
        )
        if not hwnd:
            row["ok"] = False
            row["error"] = "no shell"
            return row
        deadline = time.time() + 70.0
        dem = False
        while time.time() < deadline:
            if proc.poll() is not None:
                break
            text = err_path.read_text(encoding="utf-8", errors="replace")
            if "scene3d.present dem" in text or "present frustum" in text:
                dem = True
                time.sleep(4.0)
                break
            time.sleep(0.4)
        present, _ = find_window_by_title_substr("FlyCube Present", timeout_sec=10.0)
        target = present or hwnd
        bring_hwnd_to_front(hwnd, stay_topmost=False, restore=False)
        bmp = cap / f"scene3d-orbit-{tag}.bmp"
        ok, _near_black = capture_hwnd_bmp_ex(target, bmp, prefer_printwindow=True)
        score = score_bmp(bmp, "plugin_scene3d") if bmp.is_file() else {}
        if not _visible(score):
            ok2, _nb, _meta = capture_flycube_present_bmp(target, bmp, attempts=4)
            if ok2 and bmp.is_file():
                ok = True
                score = score_bmp(bmp, "plugin_scene3d")
        drop_topmost(target)
        row.update(
            {
                "ok": bool(ok) and _visible(score),
                "dem_log": dem,
                "title": title,
                "bmp": bmp.name,
                "landish_frac": score.get("landish_frac"),
                "navy_clear_frac": score.get("navy_clear_frac"),
                "score_ok": bool(score.get("ok")),
            }
        )
        return row
    finally:
        err.close()
        kill_exe("SmartGIS.exe")
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            pass


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="Debug")
    args = parser.parse_args(argv)
    exe = _REPO / "out" / args.out / "SmartGIS.exe"
    if not exe.is_file():
        print(f"missing {exe}", file=sys.stderr)
        return 2
    cap = _REPO / "out" / args.out / "captures" / "browser"
    log_dir = _REPO / "out" / args.out / "log"
    cap.mkdir(parents=True, exist_ok=True)
    far = _shot(exe=exe, cap=cap, log_dir=log_dir, tag="far", distance=3.2, pitch=0.52)
    near = _shot(
        exe=exe, cap=cap, log_dir=log_dir, tag="near", distance=2.95, pitch=0.62
    )
    report = {"far": far, "near": near, "ok": bool(far.get("ok") and near.get("ok"))}
    out = cap / "scene3d_orbit_pair_report.json"
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
