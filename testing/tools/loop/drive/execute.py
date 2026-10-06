# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""One suite round: env + optional private PE + inproc/os driver."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

from ..contract import Suite
from ..record.hwnd import HwndRecorder, record_enabled
from .env import prepare_env
from .inproc import run_inproc_process
from .os import run_os_process
from .plugin import restore_product_plugin_startup
from .process import is_views_exe


@dataclass
class RoundExec:
    rc: int
    inject_report: dict | None
    record_report: dict | None
    env: dict[str, str]
    run_exe: Path


def attach_recorder(
    suite: Suite, *, env: dict[str, str], captures: Path
) -> HwndRecorder | None:
    if not record_enabled(env):
        return None
    fps = 10.0
    raw_fps = str(env.get("HARNESS_RECORD_FPS", "")).strip()
    if raw_fps:
        try:
            fps = float(raw_fps)
        except ValueError:
            fps = 10.0
    return HwndRecorder(
        captures_dir=captures,
        suite_id=suite.id,
        title_substr=suite.window_title,
        fps=fps,
        find_timeout_sec=min(45.0, float(suite.timeout_sec)),
        env=env,
    )


def execute_round(
    suite: Suite,
    *,
    exe: Path,
    out: Path,
    timeout: int,
    bmp_path: Path | None,
    captures_root: Path,
) -> RoundExec:
    env = prepare_env(suite)
    run_exe = exe
    private_lock: Path | None = None
    scenic_env = (
        env.get("MAP2D_ENGINE", "").lower() == "scenic"
        or env.get("SCENE3D_ENGINE", "").lower() == "scenic"
        or os.environ.get("HARNESS_PRIVATE_EXE", "").strip()
        in ("1", "on", "true", "yes")
    )
    if scenic_env and suite.exe_name.lower().startswith("smartgisviews"):
        try:
            from .private import (
                acquire_run_lock,
                prepare_private_views_exe,
                with_private_path,
            )

            private_lock = out.parent / "scratch" / "scenic_review.lock"
            if acquire_run_lock(private_lock, timeout_sec=180.0):
                private = prepare_private_views_exe(
                    out, tag="scenic_review", exe_name=suite.exe_name
                )
                if private is not None:
                    run_exe = private
                    env = with_private_path(env, out, private)
                    print(f"private runtime: {run_exe}", flush=True)
                else:
                    private_lock = None
            else:
                print("warn: scenic private lock busy; using live exe", flush=True)
                private_lock = None
        except Exception as exc:  # noqa: BLE001
            print(f"warn: private runtime skipped ({exc})", flush=True)
            private_lock = None

    recorder = attach_recorder(suite, env=env, captures=captures_root)
    inject_report: dict | None = None
    record_report: dict | None = None
    try:
        if suite.driver == "os":
            rc, inject_report, record_report = run_os_process(
                suite,
                exe=run_exe,
                out=out,
                env=env,
                timeout=timeout,
                recorder=recorder,
                bmp_path=bmp_path,
                captures_root=captures_root,
            )
        else:
            rc, record_report = run_inproc_process(
                suite,
                exe=run_exe,
                out=out,
                env=env,
                timeout=timeout,
                recorder=recorder,
            )
    finally:
        if is_views_exe(suite.exe_name):
            restore_product_plugin_startup(run_exe)
        if private_lock is not None:
            try:
                from .private import release_run_lock

                release_run_lock(private_lock)
            except Exception:  # noqa: BLE001
                pass
    return RoundExec(
        rc=rc,
        inject_report=inject_report,
        record_report=record_report,
        env=env,
        run_exe=run_exe,
    )
