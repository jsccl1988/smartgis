# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""In-process suite round: Popen + optional HWND recorder thread."""

from __future__ import annotations

import subprocess
import threading
from pathlib import Path

from ..contract import Suite
from ..record.hwnd import HwndRecorder
from .launch import views_cmd_and_env
from .process import kill_exe, pe_launch_cwd


def run_inproc_process(
    suite: Suite,
    *,
    exe: Path,
    out: Path,
    env: dict[str, str],
    timeout: int,
    recorder: HwndRecorder | None = None,
) -> tuple[int, dict | None]:
    cmd, run_env = views_cmd_and_env(suite, exe, env)
    cwd, run_env = pe_launch_cwd(exe, run_env)
    print("RUN:", " ".join(cmd), flush=True)
    proc = subprocess.Popen(cmd, cwd=str(cwd), env=run_env)
    record_report: dict | None = None
    rec_thread = None
    if recorder is not None:
        recorder.bind_target_pid(int(proc.pid))

        def _bg_record() -> None:
            nonlocal record_report
            record_report = recorder.start_after_hwnd(wait_for_hwnd=True)

        rec_thread = threading.Thread(target=_bg_record, daemon=True)
        rec_thread.start()
    try:
        rc = proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        kill_exe(suite.exe_name)
        rc = 124
    finally:
        if rec_thread is not None:
            # Allow start_after_hwnd to finish find+burst arm (was 8s; Views
            # shell settle + map-client retarget can exceed that).
            rec_thread.join(timeout=45.0)
        if recorder is not None:
            stopped = recorder.stop()
            # stop() finalizes bmp_burst; if start never armed, keep its error.
            if (
                record_report
                and str(stopped.get("mode") or "none") == "none"
                and str(record_report.get("mode") or "none") != "none"
            ):
                record_report = {**record_report, **stopped}
            else:
                record_report = stopped
    return rc, record_report
