# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""OS-driver suite round: launch PE, bind HWND, inject, capture, record."""

from __future__ import annotations

import json
import subprocess
import time
from pathlib import Path

from . import process as process_mod
from .record.hwnd import HwndRecorder
from .suite import Suite

def _run_os_process(
    suite: Suite,
    *,
    exe: Path,
    out: Path,
    env: dict[str, str],
    timeout: int,
    recorder: HwndRecorder | None = None,
    bmp_path: Path | None = None,
    captures_root: Path | None = None,
) -> tuple[int, dict, dict | None]:
    from .interact.os_inject import run_script
    from .record.hwnd import (
        bring_hwnd_to_front,
        capture_hwnd_resilient,
        find_map_client_hwnd,
        hwnd_pid,
        is_window,
        wait_stable_shell_hwnd,
    )

    script = suite.script_path()
    if script is None or not script.is_file():
        return (
            2,
            {"ok": False, "error": "script_missing", "script": str(script)},
            None,
        )

    cmd = [str(exe), *suite.argv]
    print("RUN(os):", " ".join(cmd), flush=True)
    proc = subprocess.Popen(cmd, cwd=str(out), env=env)
    inject_report: dict = {"ok": False, "error": "inject_not_run"}
    record_report: dict | None = None
    try:
        # Optional: wait for product SaveImage BMP before inject (showcase linger).
        # legacy.browse.2d captures via capture_hwnd_bmp_ex instead — do not set
        # SMT_HARNESS_OS_WAIT_BMP for Edit-only OS suites.
        wait_bmp = str(env.get("SMT_HARNESS_OS_WAIT_BMP", "")).strip().lower() in (
            "1",
            "true",
            "yes",
            "on",
        )
        # Capture launch time so we do not treat a prior suite's BMP as ready
        # (stale china.bmp caused browse.3d inject against a dying HWND).
        proc_started = time.time()
        if wait_bmp and bmp_path is not None:
            min_bytes = suite.bmp.min_bytes if suite.bmp is not None else 1000
            deadline = time.time() + min(75.0, float(timeout))
            print(f"os-inject wait_bmp={bmp_path}", flush=True)
            # Drop stale capture so we wait for this process's write.
            try:
                if bmp_path.exists():
                    bmp_path.unlink()
            except OSError:
                pass
            while time.time() < deadline:
                try:
                    st = bmp_path.stat()
                    if (
                        st.st_size > min_bytes
                        and float(st.st_mtime) + 0.05 >= proc_started
                    ):
                        print("os-inject bmp ready", flush=True)
                        break
                except OSError:
                    pass
                if proc.poll() is not None:
                    break
                time.sleep(0.25)

        # Bind HWND to the launched PE only — title substr alone matches Cursor
        # tabs like ``legacy.browse.2d.il - smartgis - Cursor``. Also skip
        # short-lived splash HWNDs titled exactly ``SmartGis``.
        child_pid = int(proc.pid)
        # scene3d showcase HWND is 640x480 (~307k); keep floor lower than Edit.
        shell_min_area = 200_000
        if suite.window_title and "scene3d" in suite.window_title.lower():
            shell_min_area = 80_000
        hwnd, win_title = wait_stable_shell_hwnd(
            child_pid,
            title_substr=suite.window_title,
            timeout_sec=min(45.0, float(timeout)),
            min_area=shell_min_area,
            stable_ms=900,
        )
        if not hwnd:
            inject_report = {
                "ok": False,
                "error": "hwnd_timeout",
                "pid": child_pid,
                "proc_exit": proc.poll(),
            }
            print(
                f"os-inject hwnd_timeout pid={child_pid} proc_exit={proc.poll()}",
                flush=True,
            )
        else:
            inject_hwnd = int(hwnd)
            # scene3d leftover HWND has no AfxFrameOrView child — do not burn
            # the linger budget polling for a map client (was 25s → inject after
            # TerminateProcess).
            map_timeout = min(25.0, float(timeout))
            if suite.window_title and "scene3d" in suite.window_title.lower():
                map_timeout = 0.6
            map_hwnd, map_how = find_map_client_hwnd(
                int(hwnd), timeout_sec=map_timeout
            )
            if map_hwnd:
                inject_hwnd = int(map_hwnd)
                print(
                    f"os-inject map_client hwnd={inject_hwnd} via={map_how} "
                    f"(shell={hwnd} pid={child_pid} title={win_title!r})",
                    flush=True,
                )
            else:
                print(
                    f"os-inject map_client missing ({map_how}); "
                    f"using shell hwnd={hwnd} pid={child_pid} title={win_title!r}",
                    flush=True,
                )
            print(
                f"os-inject hwnd={inject_hwnd} inject={suite.os_inject_default}",
                flush=True,
            )
            bring_hwnd_to_front(int(hwnd))
            if recorder is not None:
                # Record the top-level shell (chrome + map), not only the client.
                recorder.bind_hwnd(int(hwnd), win_title or suite.window_title)
                record_report = recorder.start_after_hwnd(wait_for_hwnd=False)
                print(json.dumps({"record": record_report}, indent=2), flush=True)
            # Let chrome / deferred Edit view finish first paint before injecting.
            time.sleep(1.5)
            # Splash → main frame can replace the HWND during settle.
            if not is_window(int(hwnd)) or not is_window(int(inject_hwnd)):
                hwnd2, title2 = wait_stable_shell_hwnd(
                    child_pid,
                    title_substr=suite.window_title,
                    timeout_sec=min(20.0, float(timeout)),
                    min_area=shell_min_area,
                    stable_ms=400,
                )
                if hwnd2:
                    hwnd, win_title = hwnd2, title2 or win_title
                    map_hwnd, map_how = find_map_client_hwnd(
                        int(hwnd), timeout_sec=min(15.0, float(timeout))
                    )
                    inject_hwnd = int(map_hwnd) if map_hwnd else int(hwnd)
            # scene3d leftover has no AfxFrameOrView child — inject shell HWND.
            if not is_window(int(inject_hwnd)):
                inject_hwnd = int(hwnd)
            bring_hwnd_to_front(int(hwnd))
            if not is_window(int(inject_hwnd)):
                inject_report = {
                    "ok": False,
                    "error": "inject_hwnd_dead",
                    "pid": child_pid,
                    "shell_hwnd": int(hwnd),
                    "proc_exit": proc.poll(),
                }
                print(
                    f"os-inject inject_hwnd_dead pid={child_pid} "
                    f"proc_exit={proc.poll()}",
                    flush=True,
                )
            else:
                step_t0 = time.time()
                zoom_cfg = None
                if suite.zoom_gate is not None:
                    zoom_cfg = {
                        "before": suite.zoom_gate.before,
                        "after": suite.zoom_gate.after,
                        "settle_ms": suite.zoom_gate.settle_ms,
                    }
                click_cfg = None
                if suite.click_gate is not None and suite.click_gate.enabled:
                    click_cfg = {
                        "enabled": True,
                        "after": suite.click_gate.after,
                    }
                inject_report = run_script(
                    script,
                    inject_hwnd,
                    inject_default=suite.os_inject_default,
                    captures_root=captures_root,
                    zoom_gate=zoom_cfg,
                    click_gate=click_cfg,
                    capture_hwnd=int(hwnd),
                )
                inject_report["t_ms"] = int((time.time() - step_t0) * 1000)
                inject_report["shell_hwnd"] = int(hwnd)
                inject_report["inject_hwnd"] = int(inject_hwnd)
                inject_report["pid"] = child_pid
                inject_report["shell_pid"] = hwnd_pid(int(hwnd))
                inject_report["window_title"] = win_title
                inject_report["map_client"] = {
                    "hwnd": int(map_hwnd) if map_hwnd else 0,
                    "how": map_how,
                }
                print(json.dumps({"os_inject": inject_report}, indent=2), flush=True)

            # Suite BMP for score/review: HWND capture (not showcase SaveImage).
            if bmp_path is not None and not wait_bmp:
                # Prefer full Edit chrome for visual review. Avoid BitBlt on
                # map_client (can heap-corrupt Python); retry shell after settle.
                # legacy.browse.3d: GL present needs longer settle after maximize.
                settle = 1.2 if suite.id == "legacy.browse.3d" else 0.4
                time.sleep(settle)
                bring_hwnd_to_front(int(hwnd))
                time.sleep(0.35)
                cap_hwnd = int(hwnd)
                cap = capture_hwnd_resilient(
                    cap_hwnd,
                    Path(bmp_path),
                    settle_sec=0.35,
                    retry_sleep_sec=0.75,
                )
                frac = float(cap.get("near_black") or 1.0)
                ok = Path(bmp_path).is_file() and Path(bmp_path).stat().st_size > 1000
                # Last resort: reuse a non-black zoom_after sidecar.
                if (
                    (not ok or frac > 0.90)
                    and suite.zoom_gate is not None
                    and captures_root is not None
                ):
                    zoom_after = Path(captures_root) / suite.zoom_gate.after.replace(
                        "\\", "/"
                    )
                    zcap = (inject_report.get("zoom_capture") or {}).get("after") or {}
                    z_nb = float(zcap.get("near_black") or 1.0)
                    if (
                        zoom_after.is_file()
                        and zoom_after.stat().st_size > 1000
                        and z_nb < 0.50
                    ):
                        try:
                            Path(bmp_path).write_bytes(zoom_after.read_bytes())
                            ok, frac = True, z_nb
                            inject_report["capture_fallback"] = "zoom_after"
                        except OSError:
                            pass
                inject_report["capture"] = {
                    "ok": bool(ok),
                    "near_black": round(float(frac), 4),
                    "hwnd": cap_hwnd,
                    "path": str(bmp_path),
                    "on_primary": cap.get("on_primary"),
                    "method": "resilient",
                }
                print(json.dumps({"hwnd_capture": inject_report["capture"]}, indent=2), flush=True)
                if not ok:
                    inject_report["ok"] = False
                    inject_report.setdefault("errors", []).append("hwnd_capture_failed")

        if wait_bmp:
            # Showcase linger: allow product to exit after SaveImage + inject.
            try:
                rc = proc.wait(timeout=max(5, timeout))
            except subprocess.TimeoutExpired:
                process_mod.kill_exe(suite.exe_name)
                rc = 124
        else:
            # Bare Edit shell does not self-exit — capture then kill.
            process_mod.kill_exe(suite.exe_name)
            try:
                proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                pass
            # Intentional stop after inject/capture.
            rc = 0 if (bmp_path is not None and Path(bmp_path).is_file()) else 124
    finally:
        if recorder is not None:
            record_report = recorder.stop()
        if proc.poll() is None:
            process_mod.kill_exe(suite.exe_name)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
    return rc, inject_report, record_report



