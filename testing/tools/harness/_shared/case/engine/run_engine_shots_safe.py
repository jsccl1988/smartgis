#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Sequential engine shots with hard per-engine timeout and progress log."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


def _repo_root() -> Path:
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "build.bat").is_file() and (p / "testing").is_dir():
            return p
        p = p.parent
    raise RuntimeError("repo root not found")


def _tools_dir() -> Path:
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "loop_runner.py").is_file():
            return p
        p = p.parent
    raise RuntimeError("testing/tools not found")


ROOT = _repo_root()
OUT = ROOT / "out" / "Debug"
CASE = Path(__file__).resolve().parent
TOOLS = _tools_dir()
LOG = OUT / "engine-shots-run.log"
RUNNER = TOOLS / "loop_runner.py"
sys.path.insert(0, str(CASE))
from relabel_engine_bmps import annotate  # noqa: E402


def _suite_cmd(suite_id: str, *, rounds: int = 2) -> list[str]:
    return [
        sys.executable,
        str(RUNNER),
        "--suite",
        suite_id,
        "--no-build",
        "--rounds",
        str(rounds),
    ]


ENGINES = [
    ("views-map2d", "Views Map2D (Skia/RHI)",
     _suite_cmd("browser.map2d.china"), "map2d-showcase-china.bmp"),
    ("views-scene3d-atmosphere", "Views Scene3D (Atmosphere/FlyCube)",
     _suite_cmd("browser.world3d.full"), "atmosphere-showcase-full.bmp"),
]


def log(msg: str) -> None:
    line = msg if msg.endswith("\n") else msg + "\n"
    sys.stdout.write(line)
    sys.stdout.flush()
    with LOG.open("a", encoding="utf-8") as f:
        f.write(line)
        f.flush()


def kill_apps() -> None:
    for name in ("SmartGIS.exe", "SmartGIS-Legacy.exe", "SmartGisViews.exe", "SmartGis.exe"):
        subprocess.call(
            ["taskkill", "/F", "/IM", name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    # Views/FlyCube teardown can leave the GPU/GDI stack hot; a short pause
    # before leftover SmartGIS-Legacy.exe avoids intermittent 0xC0000374 heap fails.
    time.sleep(2.5)


def run_one(engine_id: str, label: str, cmd: list[str], src_name: str) -> dict:
    kill_apps()
    env = os.environ.copy()
    # Product default is D3D; GL shots must force OpenGL.
    if engine_id == "legacy-scene3d-gl":
        env["STEREO_API"] = "OpenGL"
        env["SCENE3D_SHOWCASE_D3D"] = "0"
    elif engine_id == "legacy-scene3d-d3d":
        env["STEREO_API"] = "Direct3D"
        env["SCENE3D_SHOWCASE_D3D"] = "1"
    else:
        env.pop("STEREO_API", None)
        env.pop("SCENE3D_SHOWCASE_D3D", None)
    env.pop("SCENE3D_SHOWCASE_LINGER_MS", None)
    env.pop("MAP2D_SHOWCASE_LINGER_MS", None)
    env["PYTHONUNBUFFERED"] = "1"

    log(f"\n######## ENGINE {engine_id} �?{label} ########")
    log("CMD: " + " ".join(cmd))
    t0 = time.time()
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ROOT),
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=180,
        )
    except subprocess.TimeoutExpired as exc:
        kill_apps()
        log(f"TIMEOUT {engine_id}: {exc}")
        return {"engine_id": engine_id, "label": label, "ok": False, "error": "timeout"}

    combined = (proc.stdout or "") + "\n" + (proc.stderr or "")
    log(combined[-2000:])
    elapsed = round(time.time() - t0, 2)
    src = OUT / src_name
    tagged = OUT / f"engine-{engine_id}.bmp"
    raw = OUT / f"engine-{engine_id}-raw.bmp"
    ok = proc.returncode == 0 and "PASS visual gates" in combined and src.exists()
    if src.exists():
        shutil.copy2(src, raw)
        shutil.copy2(raw, tagged)
        annotate(tagged, engine_id, label)
    else:
        log(f"MISSING {src}")
        ok = False
    summary = {
        "engine_id": engine_id,
        "label": label,
        "ok": ok,
        "shot_loop_rc": proc.returncode,
        "elapsed_s": elapsed,
        "tagged_bmp": str(tagged) if tagged.exists() else None,
    }
    (OUT / f"engine-{engine_id}-report.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    log(f"==> {engine_id}: {'PASS' if ok else 'FAIL'}")
    return summary


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    LOG.write_text("", encoding="utf-8")
    results = []
    for item in ENGINES:
        try:
            results.append(run_one(*item))
        except Exception as exc:  # noqa: BLE001 �?keep suite going
            log(f"EXCEPTION {item[0]}: {exc!r}")
            results.append({"engine_id": item[0], "label": item[1], "ok": False, "error": repr(exc)})
        kill_apps()
    index = OUT / "engine-shots-index.json"
    index.write_text(json.dumps(results, indent=2), encoding="utf-8")
    log("\n======== SUMMARY ========")
    for r in results:
        log(f"  [{'PASS' if r.get('ok') else 'FAIL':4}] {r['engine_id']}")
    log(f"index: {index}")
    return 0 if all(r.get("ok") for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
