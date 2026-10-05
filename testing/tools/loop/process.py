# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path


def kill_exe(image_name: str) -> None:
    subprocess.call(
        ["taskkill", "/IM", image_name, "/F"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    # Match kill_showcase_apps settle: GPU/GL driver release between rounds.
    time.sleep(1.5)


def run_process(
    cmd: list[str],
    *,
    cwd: Path,
    env: dict[str, str],
    timeout_sec: int,
    kill_image: str,
) -> int:
    print("RUN:", " ".join(cmd), flush=True)
    try:
        return subprocess.call(cmd, cwd=str(cwd), env=env, timeout=timeout_sec)
    except subprocess.TimeoutExpired:
        kill_exe(kill_image)
        return 124


def merge_env(extra: dict[str, str]) -> dict[str, str]:
    env = os.environ.copy()
    env.update(extra)
    # GDAL AutoLoadDrivers searches cwd when GDAL_DRIVER_PATH is unset. The
    # harness cwd is out/Debug (full of ucrtbased / product DLLs); loading
    # those as "drivers" duplicates the debug CRT and heap-corrupts Workspace.
    if "GDAL_DRIVER_PATH" not in env or not str(env.get("GDAL_DRIVER_PATH", "")).strip():
        root = Path(__file__).resolve().parents[3]
        env["GDAL_DRIVER_PATH"] = str(root / "out" / "third_party" / "lib" / "gdalplugins")
    return env


def switch_flag(name: str, value: str) -> str:
    key = str(name)
    if key.startswith("SMT_"):
        key = key[4:]
    elif key.startswith("SG_"):
        key = key[3:]
    key = key.replace("_", "-").lower()
    return f"--{key}={value}"


def is_product_switch_key(key: str) -> bool:
    k = str(key)
    if k.startswith("SMT_") or k.startswith("SG_"):
        return True
    if k in {"SMARTGIS_ROOT", "CURSOR_API_KEY"}:
        return True
    if k.islower() and "-" in k:
        return True
    return False


def peel_product_switches(env: dict[str, str]) -> tuple[dict[str, str], list[str]]:
    kept: dict[str, str] = {}
    flags: list[str] = []
    for k, v in env.items():
        if is_product_switch_key(k):
            flags.append(switch_flag(k, str(v)))
        else:
            kept[k] = v
    return kept, flags


def is_views_exe(exe_name: str) -> bool:
    return str(exe_name).lower().startswith("smartgisviews")
