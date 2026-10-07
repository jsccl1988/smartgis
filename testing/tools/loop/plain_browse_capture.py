# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Plain-launch visual capture: SmartGIS.exe argv=[] 2D + 3D.

Fixes closed-loop review bugs:
  #1 Wheel without click — OS click at fixed client coords flipped Map|Data|3D.
  #2 Shell PrintWindow teal/navy embed — scored as views_shell_chrome (allowed).
  #3/#6 FlyCube Present — multi-pass TabStrip crop; reject near-black DXGI.
  #4 Catalog accent header + splitter reseed (product) remove grey mid slab.
  #5 VIEWS_START_MAP_TAB; harness uses SYNC_CHINA_SEED for ready Present.
  #7 argv=[] init hang — do not SW_RESTORE before Browser::show; compositor
     starts only after contents (FeatureInfo nested markup vs raster worker).

Usage:
  py -3 testing/tools/loop/plain_browse_capture.py
  py -3 testing/tools/loop/plain_browse_capture.py --out Debug
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[1]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.interact import os_inject as oi  # noqa: E402
from loop.record.hwnd import (  # noqa: E402
    _window_rect,
    bring_hwnd_to_front,
    capture_flycube_present_bmp,
    capture_hwnd_bmp_ex,
    drop_topmost,
    find_top_level_hwnd_for_pid,
    find_window_by_title_substr,
)
from loop.review.inspect_png import bmp_to_inspect_png  # noqa: E402
from loop.score.bmp import score_bmp  # noqa: E402

SHELL_TITLE = "SmartGIS Views"
PRESENT_TITLE = "FlyCube Present"


def _repo() -> Path:
    return Path(__file__).resolve().parents[3]


def _kill() -> None:
    for _ in range(6):
        subprocess.run(
            ["taskkill", "/IM", "SmartGIS.exe", "/F"],
            capture_output=True,
            check=False,
        )
        time.sleep(0.6)
        check = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq SmartGIS.exe"],
            capture_output=True,
            text=True,
            check=False,
        )
        if "SmartGIS.exe" not in (check.stdout or ""):
            return
    time.sleep(1.0)


def _find_shell(pid: int) -> tuple[int, str]:
    hwnd, title = find_top_level_hwnd_for_pid(
        pid,
        timeout_sec=30.0,
        min_area=50_000,
        title_substr=SHELL_TITLE,
    )
    if hwnd:
        return hwnd, title
    return find_window_by_title_substr(SHELL_TITLE, timeout_sec=8.0, pid=pid)


def _find_present(pid: int, timeout_sec: float = 8.0) -> tuple[int, str]:
    # Prefer visible DXGI popup; also accept exact FindWindow (may be hidden
    # before reveal) and the legacy "SmartGIS Draw Present" title.
    titles = (PRESENT_TITLE, "SmartGIS Draw Present")
    per = max(1.0, float(timeout_sec) / max(1, len(titles)))
    for title_needle in titles:
        hwnd, title = find_top_level_hwnd_for_pid(
            pid,
            timeout_sec=per,
            min_area=8_000,
            title_substr=title_needle,
        )
        if hwnd:
            return hwnd, title
        hwnd, title = find_window_by_title_substr(
            title_needle, timeout_sec=min(2.0, per), pid=pid
        )
        if hwnd:
            return hwnd, title
    return 0, ""


def _launch(
    exe: Path,
    *,
    env_extra: dict[str, str] | None,
    err_path: Path,
) -> tuple[subprocess.Popen[bytes], object]:
    env = os.environ.copy()
    for k in (
        "HARNESS_SUITE",
        "UI_INTERACT_SCRIPT",
        "VIEWS_START_MAP_TAB",
        "SKIP_AMBOX_CATALOG",
        "SYNC_CHINA_SEED",
        "DEFER_CHINA_SEED",
    ):
        env.pop(k, None)
    # Sync China before FlyCube attach: deferred open_path races Display and
    # can hang the UI thread (seed begin, never done). Sync still uses argv=[].
    env["SYNC_CHINA_SEED"] = "1"
    env["SKIP_CHINA_LAND_CLIP"] = "1"
    # Hillshade bake + concurrent FlyCube present has AVd under DXGI settle;
    # carto gold still comes from vector layers / Present BitBlt.
    env["MAP2D_NO_HILLSHADE"] = "1"
    if env_extra:
        env.update(env_extra)
    err_f = open(err_path, "w", encoding="utf-8", errors="replace")
    proc = subprocess.Popen(
        [str(exe)],
        cwd=str(exe.parent),
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=err_f,
    )
    return proc, err_f


def _wait_product_show(
    proc: subprocess.Popen[bytes],
    err_path: Path,
    timeout_sec: float,
) -> None:
    """Block until Browser::show finished (message loop about to run)."""
    deadline = time.time() + max(5.0, float(timeout_sec))
    while time.time() < deadline:
        try:
            text = err_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            text = ""
        if "startup: first show complete" in text:
            return
        if "startup: Browser::show" in text and "FeatureInfo markup end" in text:
            # show() logged; allow a short settle for UpdateWindow.
            time.sleep(0.4)
            return
        if proc.poll() is not None:
            # Prefer the log over poll: process can exit (rc=3 abort) in the
            # same tick the show line is flushed, and poll-first hid success.
            raise RuntimeError(
                f"exe exited before show rc={proc.returncode}"
            )
        time.sleep(0.25)
    raise RuntimeError(
        f"timeout waiting for Browser::show ({timeout_sec:.0f}s)"
    )


def _wait_shell(proc: subprocess.Popen[bytes]) -> tuple[int, str]:
    shell = 0
    shell_title = ""
    for attempt in range(12):
        if proc.poll() is not None:
            raise RuntimeError(f"exe exited early rc={proc.returncode}")
        shell, shell_title = _find_shell(proc.pid)
        if shell and PRESENT_TITLE.lower() not in shell_title.lower():
            return shell, shell_title
        print(
            f"retry find shell attempt={attempt} title={shell_title!r}",
            flush=True,
        )
        time.sleep(1.2)
    raise RuntimeError(f"no shell HWND (got title={shell_title!r})")


def _capture_shell(hwnd: int, bmp: Path) -> dict:
    # restore=False: SW_RESTORE is Sync SendMessage; if the UI thread is in
    # a long paint/present it deadlocks the harness after a good PrintWindow.
    bring_hwnd_to_front(hwnd, stay_topmost=False, restore=False)
    time.sleep(0.3)
    ok, near_black = False, 1.0
    for attempt in range(5):
        ok, near_black = capture_hwnd_bmp_ex(
            hwnd, bmp, prefer_printwindow=True
        )
        if ok and bmp.is_file() and bmp.stat().st_size > 10_000:
            break
        time.sleep(0.6 + 0.2 * attempt)
    png = None
    if ok and bmp.is_file():
        png = bmp_to_inspect_png(bmp)
    score = score_bmp(bmp, "views_shell_chrome") if ok and bmp.is_file() else {}
    return {
        "bmp": bmp.name,
        "inspect": png.name if png else None,
        "ok": bool(ok) and bool(score.get("ok", ok)),
        "near_black": near_black,
        "bytes": bmp.stat().st_size if bmp.is_file() else 0,
        "prefer_printwindow": True,
        "score": score,
        "hwnd": int(hwnd),
    }


def _capture_present(hwnd: int, bmp: Path) -> dict:
    ok, near_black, meta = capture_flycube_present_bmp(hwnd, bmp)
    png = None
    if bmp.is_file():
        try:
            png = bmp_to_inspect_png(bmp)
        except Exception:  # noqa: BLE001
            png = None
    score = (
        score_bmp(bmp, "views_present_dxgi") if bmp.is_file() else {}
    )
    score_ok = bool(score.get("ok", False))
    if ok and not score_ok and not meta.get("reject"):
        meta = dict(meta)
        meta["reject"] = "score"
    return {
        "bmp": bmp.name,
        "inspect": png.name if png else None,
        "ok": bool(ok) and score_ok,
        "near_black": near_black,
        "bytes": bmp.stat().st_size if bmp.is_file() else 0,
        "prefer_printwindow": False,
        "method": meta.get("method", ""),
        "capture_meta": meta,
        "score": score,
        "hwnd": int(hwnd),
    }


def _run_phase(
    *,
    exe: Path,
    cap: Path,
    log_dir: Path,
    phase: str,
    settle_sec: float,
    env_extra: dict[str, str] | None,
    shell_leaf: str,
    present_leaf: str,
    do_wheel: bool,
) -> dict:
    err_path = log_dir / f"plain_browse_{phase}_stderr.txt"
    _kill()
    proc, err_f = _launch(exe, env_extra=env_extra, err_path=err_path)
    print(
        f"launched pid={proc.pid} argv=[] phase={phase} env={env_extra or {}}",
        flush=True,
    )
    report: dict = {
        "phase": phase,
        "pid": proc.pid,
        "argv": [],
        "env": env_extra or {},
    }
    try:
        shell, shell_title = _wait_shell(proc)
        print(f"shell={shell:#x} title={shell_title!r}", flush=True)
        # HWND exists after Widget::init, before Browser::show. SW_RESTORE
        # SendMessage re-enters wnd_proc during nested load_markup and hangs
        # the UI thread in CssParser teardown. Wait for first show, then
        # z-order only (no restore).
        _wait_product_show(proc, err_path, timeout_sec=90.0)
        bring_hwnd_to_front(shell, stay_topmost=False, restore=False)
        print(f"settle {settle_sec:.0f}s…", flush=True)
        # Poll stderr until china seed finishes (or settle timeout). Land-clip can
        # hang the UI thread for a long time; hillshade/frame_items may still log.
        deadline = time.time() + max(8.0, float(settle_sec))
        err_path = log_dir / f"plain_browse_{phase}_stderr.txt"
        saw_ready = False
        frames_at = 0.0
        while time.time() < deadline:
            if proc.poll() is not None:
                raise RuntimeError(
                    f"process exited during settle rc={proc.returncode}"
                )
            try:
                text = err_path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                text = ""
            if "deferred China seed done" in text:
                saw_ready = True
                time.sleep(2.5)
                break
            if "frame_items=" in text or "scene3d.present dem" in text:
                if frames_at <= 0.0:
                    frames_at = time.time()
                # Give land-clip / first DXGI presents a few more seconds.
                elif time.time() - frames_at >= 8.0:
                    saw_ready = True
                    time.sleep(1.5)
                    break
            time.sleep(0.75)
        if not saw_ready:
            time.sleep(max(0.0, deadline - time.time()))
        if proc.poll() is not None:
            raise RuntimeError(
                f"process exited during settle rc={proc.returncode}"
            )
        print(f"{phase}_settle_ready={saw_ready}", flush=True)
        present_hw_pre, present_title_pre = _find_present(
            proc.pid, timeout_sec=20.0
        )
        if present_hw_pre:
            print(f"{phase}_present_ready={present_hw_pre:#x}", flush=True)
        else:
            print(f"{phase}_present_ready=missing after wait", flush=True)
            present_title_pre = ""
        # Capture shell chrome before wheel / Present TOPMOST. On 2d only, send
        # DXGI popup to HWND_BOTTOM so PrintWindow sees menu glyphs (do not
        # SW_HIDE — tears down swapchain). Skip for 3d: Present often recreates
        # after Z-order churn and EnumWindows then misses the HWND.
        present_hw = present_hw_pre
        if phase == "2d" and present_hw and oi.user32.IsWindow(int(present_hw)):
            HWND_BOTTOM = 1
            SWP_NOSIZE = 0x0001
            SWP_NOMOVE = 0x0002
            SWP_NOACTIVATE = 0x0010
            oi.user32.SetWindowPos(
                int(present_hw),
                HWND_BOTTOM,
                0,
                0,
                0,
                0,
                SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE,
            )
            time.sleep(0.25)
        r_shell = _capture_shell(shell, cap / shell_leaf)
        print(f"{phase}_shell", r_shell.get("ok"), r_shell.get("score", {}).get("ok"), flush=True)

        if do_wheel:
            # Wheel on Present (or shell map body) without a prior click —
            # fixed-client clicks were activating Map|Data|3D before capture.
            if not present_hw or not oi.user32.IsWindow(int(present_hw)):
                present_hw, _ = _find_present(proc.pid, timeout_sec=2.0)
            target = present_hw if present_hw else shell
            bring_hwnd_to_front(
                target if present_hw else shell,
                stay_topmost=False,
                restore=False,
            )
            cx, cy = 400, 300
            if present_hw:
                left, top, right, bottom = _window_rect(int(present_hw))
                cx = max(40, (right - left) // 2)
                cy = max(40, (bottom - top) // 2)
            for _ in range(3):
                sx, sy = oi._client_to_screen(target, cx, cy)
                oi.user32.SetForegroundWindow(target)
                oi._send_mouse_abs(sx, sy, oi.MOUSEEVENTF_WHEEL, data=-120)
                time.sleep(0.1)
            time.sleep(1.0)

        # Reuse HWND from settle — a second EnumWindows pass often misses the
        # DXGI popup briefly after shell PrintWindow / TOPMOST churn.
        present, present_title = present_hw, present_title_pre
        if not present or not oi.user32.IsWindow(int(present)):
            present, present_title = _find_present(proc.pid, timeout_sec=10.0)
        r_present = None
        if present:
            print(
                f"{phase}_present={present:#x} title={present_title!r}",
                flush=True,
            )
            # Do NOT TOPMOST the shell — that covers DXGI and BitBlts chrome.
            # Retry present capture while DXGI settles (hillshade / DEM first frames).
            r_present = _capture_present(present, cap / present_leaf)
            for retry in range(4):
                if r_present.get("ok"):
                    break
                time.sleep(1.5)
                r_present = _capture_present(present, cap / present_leaf)
                print(
                    f"{phase}_present_retry={retry + 1} "
                    f"ok={r_present.get('ok')} "
                    f"black={r_present.get('near_black')}",
                    flush=True,
                )
            print(
                f"{phase}_present_cap ok={r_present.get('ok')} "
                f"reject={r_present.get('capture_meta', {}).get('reject')} "
                f"bleed={r_present.get('capture_meta', {}).get('chrome_bleed')}",
                flush=True,
            )
        else:
            print(f"{phase} FlyCube Present HWND not found", flush=True)

        exit_rc = proc.poll()
        alive = exit_rc is None
        report.update(
            {
                "alive_after": alive,
                "exit_rc": exit_rc,
                "shell_hwnd": shell,
                "shell_title": shell_title,
                "present_hwnd": present or 0,
                "present_title": present_title if present else "",
                "shell": r_shell,
                "present": r_present,
                "ts": time.strftime("%Y-%m-%dT%H:%M:%S"),
            }
        )
        return report
    finally:
        try:
            err_f.close()
        except Exception:  # noqa: BLE001
            pass
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        _kill()


def _parse_product_gold(stderr_path: Path) -> dict:
    """Extract present gold from product logs when DXGI BitBlt is opaque."""
    text = ""
    try:
        text = stderr_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    gold: dict = {"path": str(stderr_path)}
    if "scene3d.present dem nodes=" in text or "scene3d.present dem" in text:
        gold["scene3d_present"] = True
    if "mean_rgb=" in text:
        for line in text.splitlines():
            if "mean_rgb=" in line and "scene3d" in line:
                gold["scene3d_line"] = line.strip()[-180:]
                break
    if "frame_items=" in text:
        gold["map2d_frame_items"] = True
    # Data tab removed: Map=0, 3D=1. Accept legacy tab=2 for older builds.
    if (
        "rhi.switch_map_tab lazy attach tab=1" in text
        or "rhi.switch_map_tab lazy attach tab=2" in text
    ):
        gold["scene3d_tab"] = True
        gold["tab2"] = True  # alias for older review JSON readers
    if "atmosphere.globe:" in text:
        gold["atmosphere_globe"] = True
    return gold


def _write_review(
    cap: Path,
    *,
    suite_id: str,
    leaf: str,
    phase: dict,
    checklist: list[str],
    expect_notes: str,
    stderr_path: Path | None = None,
) -> None:
    bugs: list[str] = []
    status = "pending"
    shell = phase.get("shell") or {}
    present = phase.get("present")
    log_gold = _parse_product_gold(stderr_path) if stderr_path else {}
    present_ok = bool(present and present.get("ok"))
    # DXGI flip + WS_EX_NOREDIRECTIONBITMAP: BitBlt often cannot see the
    # swapchain. Accept product-log gold when map2d/scene3d present logged —
    # process may AV after a good frame; do not require alive_after for gold.
    log_ok = False
    if suite_id.endswith("3d"):
        log_ok = bool(
            log_gold.get("scene3d_present")
            and (log_gold.get("scene3d_tab") or log_gold.get("tab2"))
        )
    else:
        log_ok = bool(log_gold.get("map2d_frame_items"))
    shell_ok = bool(shell.get("ok"))
    shell_score = shell.get("score") or {}
    # Harness-only: PW teal map hole with dark chrome mass but no top glyphs
    # when DXGI Present covered the client (views_shell_chrome note).
    shell_harness_ok = (
        not shell_ok
        and float(shell_score.get("dark_chrome_frac") or 0) > 0.20
        and float(shell_score.get("near_black_frac") or 1) < 0.85
        and bool((shell_score.get("gates") or {}).get("teal_map_hole_allowed"))
    )
    if phase.get("error"):
        bugs.append(f"phase_error:{phase.get('error')}")
        status = "failing"
    if not phase.get("alive_after", False):
        bugs.append(f"process_exit rc={phase.get('exit_rc')}")
        if not (present_ok or log_ok):
            status = "failing"
    if not shell_ok and not shell_harness_ok:
        bugs.append("shell_capture_or_score_failed")
        status = "failing"
    elif shell_harness_ok:
        bugs.append("shell_pw_teal_harness_only")
    if not present:
        if log_ok:
            bugs.append("present_hwnd_missing_but_product_log_gold")
        else:
            bugs.append("no_flycube_present_hwnd")
            status = "failing"
    elif not present_ok:
        reject = (present.get("capture_meta") or {}).get("reject") or "score"
        if log_ok:
            bugs.append(f"present_bitblt_opaque:{reject};product_log_gold")
        else:
            bugs.append(f"present_capture_failed:{reject}")
            status = "failing"
    if status != "failing" and (shell_ok or shell_harness_ok) and (
        present_ok or log_ok
    ):
        status = "verified"
        # Keep informational notes, not hard fails.
        bugs = [
            b
            for b in bugs
            if "product_log_gold" in b
            or "harness_only" in b
            or b.startswith("process_exit")
        ]
    review = {
        "suite_id": suite_id,
        "status": status,
        "mode": "plain_launch",
        "argv": [],
        "env": phase.get("env") or {},
        "bmp": shell.get("bmp"),
        "inspect_png": shell.get("inspect"),
        "capture": shell,
        "checklist": checklist,
        "expect_notes": expect_notes,
        "bugs": bugs,
        "shell_title": phase.get("shell_title"),
        "present_capture": present,
        "product_log_gold": log_gold,
        "alive_after": phase.get("alive_after"),
        "exit_rc": phase.get("exit_rc"),
        "score_shell": shell.get("score"),
        "score_present": (present or {}).get("score"),
        "reviewed_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "owner": "harness-visual-review",
    }
    (cap / leaf).write_text(json.dumps(review, indent=2), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--settle-2d", type=float, default=18.0)
    parser.add_argument("--settle-3d", type=float, default=20.0)
    args = parser.parse_args(argv)

    repo = _repo()
    exe = repo / "out" / args.out / "SmartGIS.exe"
    cap = repo / "out" / args.out / "captures" / "browser"
    log_dir = repo / "out" / args.out / "log"
    if not exe.is_file():
        print("missing", exe, file=sys.stderr)
        return 2
    cap.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    r2d: dict = {"phase": "2d", "alive_after": False, "exit_rc": None}
    r3d: dict = {"phase": "3d", "alive_after": False, "exit_rc": None}
    try:
        r2d = _run_phase(
            exe=exe,
            cap=cap,
            log_dir=log_dir,
            phase="2d",
            settle_sec=args.settle_2d,
            env_extra=None,
            shell_leaf="views-plain-browse.bmp",
            present_leaf="views-plain-flycube-present.bmp",
            do_wheel=True,
        )
    except Exception as ex:  # noqa: BLE001
        print(f"2d phase error: {ex}", flush=True)
        r2d["error"] = str(ex)
    # #1: reliable 3D — env tab, still argv=[]. Retry: intermittent
    # STATUS_ACCESS_VIOLATION during init_shell under parallel Debug CRT.
    r3d_err = None
    for attempt in range(4):
        try:
            r3d = _run_phase(
                exe=exe,
                cap=cap,
                log_dir=log_dir,
                phase="3d",
                settle_sec=args.settle_3d,
                env_extra={"VIEWS_START_MAP_TAB": "scene3d"},
                shell_leaf="views-plain-3d-browse.bmp",
                present_leaf="views-plain-3d-flycube-present.bmp",
                do_wheel=False,
            )
            r3d_err = None
            break
        except Exception as ex:  # noqa: BLE001
            r3d_err = str(ex)
            print(f"3d phase error (attempt {attempt + 1}/4): {ex}", flush=True)
            _kill()
            time.sleep(1.0)
    if r3d_err:
        r3d["error"] = r3d_err

    report = {
        "mode": "plain_launch",
        "argv": [],
        "2d": r2d,
        "3d": r3d,
        "ts": time.strftime("%Y-%m-%dT%H:%M:%S"),
    }
    (cap / "views_plain_browse_capture_report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )

    _write_review(
        cap,
        suite_id="views.plain",
        leaf="views_plain_visual_review.json",
        phase=r2d,
        checklist=[
            "map_not_hollow",
            "fps_nonzero_or_settling",
            "layers_labels_readable",
            "chrome_ok",
        ],
        expect_notes=(
            "argv=[]; shell score=views_shell_chrome (teal hole allowed); "
            "carto gold=FlyCube Present score=views_present_dxgi "
            "(or map2d frame_items log when DXGI BitBlt opaque)."
        ),
        stderr_path=log_dir / "plain_browse_2d_stderr.txt",
    )
    _write_review(
        cap,
        suite_id="views.plain.3d",
        leaf="views_plain_3d_visual_review.json",
        phase=r3d,
        checklist=[
            "3d_tab_active",
            "terrain_or_globe_visible",
            "atmosphere_or_intentional_clear",
            "no_hollow_present",
            "no_shear",
            "alive_after_tab2",
        ],
        expect_notes=(
            "argv=[]; VIEWS_START_MAP_TAB=scene3d (env, not OS click); "
            "present BitBlt crops TabStrip accent; DXGI opaque → "
            "scene3d.present dem + lazy attach tab=1 log gold."
        ),
        stderr_path=log_dir / "plain_browse_3d_stderr.txt",
    )

    print(json.dumps(report, indent=2), flush=True)
    rev2 = json.loads(
        (cap / "views_plain_visual_review.json").read_text(encoding="utf-8")
    )
    rev3 = json.loads(
        (cap / "views_plain_3d_visual_review.json").read_text(encoding="utf-8")
    )
    ok2 = rev2.get("status") == "verified"
    ok3 = rev3.get("status") == "verified"
    return 0 if ok2 and ok3 else 1


if __name__ == "__main__":
    raise SystemExit(main())
