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


def pe_launch_cwd(exe: Path, env: dict[str, str]) -> tuple[Path, dict[str, str]]:
    """cwd must not be the PE directory (out/Debug is full of CRT/product DLLs).

    GDAL AutoLoadDrivers LoadLibrary's every DLL in cwd; a second ucrtbased
    heap-corrupts PluginShell / CommandCatalog.
    """
    run_env = dict(env)
    exe_dir = exe.resolve().parent
    run_env["PATH"] = str(exe_dir) + os.pathsep + str(run_env.get("PATH", ""))
    cwd = exe_dir / "captures" / "_scratch" / "gdal_cwd"
    cwd.mkdir(parents=True, exist_ok=True)
    run_env["GDAL_DRIVER_PATH"] = "disable"
    return cwd, run_env


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
    # AutoLoadDrivers searches cwd unless GDAL_DRIVER_PATH is a real directory.
    # drive/process.py → repo is parents[4]. A missing folder still scans
    # out/Debug and LoadLibrary's ucrtbased (heap corruption).
    # "disable" is a GDAL sentinel (skip AutoLoadDrivers). Do not replace it
    # with a Path that fails is_dir() and then fall back to out/Debug.
    raw = str(env.get("GDAL_DRIVER_PATH", "")).strip()
    if raw.lower() == "disable":
        return env
    root = Path(__file__).resolve().parents[4]
    plugins = root / "out" / "Debug" / "gdalplugins"
    plugins.mkdir(parents=True, exist_ok=True)
    current = Path(raw)
    if not current.is_dir():
        env["GDAL_DRIVER_PATH"] = str(plugins)
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
    name = str(exe_name).lower()
    # GPU/render child is SmartGisRender.exe — keep its --self-test argv.
    if name.startswith("smartgisrender"):
        return False
    return name.startswith("smartgis")
