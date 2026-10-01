# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Kill only showcase / self-test PE instances - never interactive double-click."""

from __future__ import annotations

import subprocess
import time
from pathlib import Path

_SHOWCASE_MARKERS = (
    "--map2d-showcase",
    "--atmosphere-showcase",
    "--scene3d-showcase",
    "--ui-showcase",
    "--self-test",
    "--input-showcase",
    "--plugin-showcase",
    "--browse-showcase",
    # GPU / utility children of harness runs (no showcase flag on cmdline).
    "--type=gpu",
    "--type=renderer",
    "--type=utility",
)

_IMAGES = ("SmartGis.exe", "SmartGisViews.exe")


def kill_showcase_apps(*, settle_sec: float = 1.5) -> int:
    """Force-stop SmartGis* processes whose cmdline looks like a shot/self-test.

    Also stops leftover --type=gpu/renderer children that hold the PE lock
    after the parent showcase/self-test is killed (LNK1168 / WinError 32).
    """
    ps = r"""
$markers = @('--map2d-showcase','--atmosphere-showcase','--scene3d-showcase','--ui-showcase','--self-test','--input-showcase','--plugin-showcase','--browse-showcase','--type=gpu','--type=renderer','--type=utility')
$names = @('SmartGis.exe','SmartGisViews.exe')
$n = 0
Get-CimInstance Win32_Process |
  Where-Object { $names -contains $_.Name -and $_.CommandLine } |
  ForEach-Object {
    $cmd = $_.CommandLine
    $hit = $false
    foreach ($m in $markers) {
      if ($cmd -like ('*' + $m + '*')) { $hit = $true; break }
    }
    if ($hit) {
      Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
      $n++
    }
  }
Write-Output $n
"""
    try:
        out = subprocess.check_output(
            ["powershell", "-NoProfile", "-Command", ps],
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=30,
        )
        killed = int((out or "0").strip().splitlines()[-1] or "0")
    except (subprocess.SubprocessError, ValueError, OSError):
        killed = 0
    if settle_sec > 0:
        time.sleep(settle_sec)
    return killed


def kill_pid_tree(pid: int) -> None:
    if pid <= 0:
        return
    subprocess.call(
        ["taskkill", "/F", "/T", "/PID", str(pid)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def wait_exe_ready(
    exe: Path,
    *,
    timeout_sec: float = 90.0,
    also_wait_build_lock: bool = True,
) -> bool:
    """Wait until ``exe`` is readable and (optionally) debug compile is idle.

    Does **not** kill running showcases — concurrent agents' waiters must not
    TerminateProcess each other's ``--ui-showcase`` / ``--browse-showcase``.
    Round start still calls ``kill_showcase_apps`` once via ``_kill``.
    """
    root = exe.resolve().parents[1]  # out/<config> -> out
    lock = root / ".build.lock.debug"
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if also_wait_build_lock and lock.is_file():
            time.sleep(1.5)
            continue
        if not exe.is_file():
            time.sleep(1.0)
            continue
        try:
            with open(exe, "rb"):
                return True
        except OSError:
            time.sleep(1.0)
    return False
