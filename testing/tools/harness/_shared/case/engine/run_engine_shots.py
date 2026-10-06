#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run each Views showcase engine once; tag BMP + JSON by engine name.

Writes under out/Debug:
  engine-<id>-<stem>.bmp   (copy of the showcase BMP with a title bar label)
  engine-<id>-report.json

Exit 0 only if every configured engine PASSes its visual gates.
"""

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
RUNNER = TOOLS / "loop_runner.py"


def _suite_cmd(suite_id: str, *, rounds: int = 3) -> list[str]:
    return [
        sys.executable,
        str(RUNNER),
        "--suite",
        suite_id,
        "--no-build",
        "--rounds",
        str(rounds),
    ]


# (engine_id, human label, argv for the shot loop, source BMP produced by that loop)
ENGINES: list[tuple[str, str, list[str], str]] = [
    (
        "views-map2d",
        "Views Map2D (Skia/RHI)",
        _suite_cmd("browser.map2d.china"),
        "map2d-showcase-china.bmp",
    ),
    (
        "views-scene3d-atmosphere",
        "Views Scene3D (Atmosphere/FlyCube)",
        _suite_cmd("browser.world3d.full"),
        "atmosphere-showcase-full.bmp",
    ),
]


def kill_apps() -> None:
    # Only showcase/self-test instances �?never interactive double-click.
    if str(CASE) not in sys.path:
        sys.path.insert(0, str(CASE))
    from kill_showcase import kill_showcase_apps

    kill_showcase_apps(settle_sec=2.5)


def annotate_bmp(src: Path, dst: Path, engine_id: str, label: str) -> None:
    """Copy BMP and burn engine id into a top title strip (pure Python)."""
    if str(CASE) not in sys.path:
        sys.path.insert(0, str(CASE))
    from relabel_engine_bmps import annotate as burn_label

    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    burn_label(dst, engine_id, label)


def parse_loop_ok(stdout: str) -> bool:
    return "PASS " in stdout and " gates" in stdout


def run_one(engine_id: str, label: str, cmd: list[str], src_bmp: str) -> dict:
    kill_apps()
    env = os.environ.copy()
    # Product default is D3D; GL shots must force OpenGL. D3D engine leaves
    # env to the --d3d shot-loop path (or inherits Direct3D).
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

    print(f"\n######## ENGINE {engine_id} �?{label} ########", flush=True)
    print("CMD:", " ".join(cmd), flush=True)
    started = time.time()
    proc = subprocess.run(
        cmd,
        cwd=str(ROOT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    elapsed = round(time.time() - started, 2)
    # Shot loops print JSON + PASS/FAIL to stdout (and some to stderr).
    combined = (proc.stdout or "") + "\n" + (proc.stderr or "")
    try:
        print(combined[-2500:], flush=True)
    except UnicodeEncodeError:
        print(combined[-2500:].encode("ascii", "replace").decode("ascii"), flush=True)

    src = OUT / src_bmp
    tagged_bmp = OUT / f"engine-{engine_id}.bmp"
    report_path = OUT / f"engine-{engine_id}-report.json"
    ok = proc.returncode == 0 and parse_loop_ok(combined) and src.exists()
    if src.exists():
        # Snapshot before annotate so shared stems (GL/D3D china bmp) stay
        # engine-specific even if a later engine overwrites the showcase path.
        raw_snap = OUT / f"engine-{engine_id}-raw.bmp"
        shutil.copy2(src, raw_snap)
        annotate_bmp(raw_snap, tagged_bmp, engine_id, label)
    else:
        print(f"MISSING source BMP: {src}", flush=True)
        ok = False

    summary = {
        "engine_id": engine_id,
        "label": label,
        "ok": ok,
        "shot_loop_rc": proc.returncode,
        "elapsed_s": elapsed,
        "source_bmp": str(src) if src.exists() else None,
        "tagged_bmp": str(tagged_bmp) if tagged_bmp.exists() else None,
    }
    # Try pull last JSON object from loop stdout.
    try:
        start = combined.rfind("{")
        end = combined.rfind("}")
        if start >= 0 and end > start:
            summary["score"] = json.loads(combined[start : end + 1])
    except json.JSONDecodeError:
        pass
    report_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(
        f"==> {engine_id}: {'PASS' if ok else 'FAIL'}  "
        f"bmp={tagged_bmp.name if tagged_bmp.exists() else 'n/a'}",
        flush=True,
    )
    return summary


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    results: list[dict] = []
    for engine_id, label, cmd, src_bmp in ENGINES:
        results.append(run_one(engine_id, label, cmd, src_bmp))
    kill_apps()

    index = OUT / "engine-shots-index.json"
    index.write_text(json.dumps(results, indent=2), encoding="utf-8")
    print("\n======== SUMMARY ========", flush=True)
    for r in results:
        print(
            f"  [{('PASS' if r['ok'] else 'FAIL'):4}] {r['engine_id']:28}  "
            f"{r.get('tagged_bmp')}",
            flush=True,
        )
    print(f"index: {index}", flush=True)
    return 0 if all(r["ok"] for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
