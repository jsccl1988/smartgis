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
    time.sleep(0.4)


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
    return env
