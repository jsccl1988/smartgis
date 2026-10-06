# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Kill / wait / stale-artifact cleanup at the start of a suite round."""

from __future__ import annotations

from pathlib import Path

from ..contract import Suite
from . import process as process_mod


def kill_round(suite: Suite) -> None:
    if suite.kill_showcase:
        try:
            from .kill import kill_showcase_apps

            # 4s: OpenGL ICD needs settle after TerminateProcess / taskkill
            # or the next scene3d round exits early with empty marks / bmp_missing.
            settle = 4.0
            sid = suite.id if suite else ""
            if sid.startswith("plugin."):
                settle = 8.0
            kill_showcase_apps(settle_sec=settle)
        except Exception as exc:  # noqa: BLE001
            print(f"warn: kill_showcase failed ({exc}); falling back", flush=True)
    process_mod.kill_exe(suite.exe_name)


def wait_exe_ready(suite: Suite, *, config: str) -> bool:
    try:
        from .kill import wait_exe_ready as wait_ready

        exe = suite.exe_path(config)
        if not exe.is_file():
            return False
        ok = wait_ready(exe, timeout_sec=90.0)
        if not ok:
            print(f"warn: exe not ready within timeout: {exe}", flush=True)
        return ok
    except Exception as exc:  # noqa: BLE001
        print(f"warn: wait_exe_ready failed ({exc})", flush=True)
        return suite.exe_path(config).is_file()


def unlink_stale_probes(
    suite: Suite,
    *,
    mark: Path | None,
    bmp: Path | None,
    review_prep: bool,
    force_run: bool,
) -> None:
    if mark is not None and mark.exists():
        try:
            mark.unlink()
        except OSError:
            pass
    if bmp is not None and bmp.exists() and not (review_prep and not force_run):
        try:
            bmp.unlink()
        except OSError as exc:
            print(f"warn: could not delete stale BMP: {exc}", flush=True)
