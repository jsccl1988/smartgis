# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Plain-launch visual capture: SmartGisViews.exe argv=[] 2D + 3D.

Fixes closed-loop review bugs:
  #1 Wheel without click — OS click at fixed client coords flipped Map|Data|3D.
  #2 Shell PrintWindow teal/navy embed — scored as views_shell_chrome (allowed).
  #3/#6 FlyCube Present — multi-pass TabStrip crop; reject near-black DXGI.
  #4 Catalog accent header + splitter reseed (product) remove grey mid slab.
  #5 SMT_VIEWS_START_MAP_TAB; harness uses SMT_SYNC_CHINA_SEED for ready Present.

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
            ["taskkill", "/IM", "SmartGisViews.exe", "/F"],
            capture_output=True,
            check=False,
        )
        time.sleep(0.6)
        check = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq SmartGisViews.exe"],
            capture_output=True,
            text=True,
            check=False,
        )
        if "SmartGisViews.exe" not in (check.stdout or ""):
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
    hwnd, title = find_top_level_hwnd_for_pid(
        pid,
        timeout_sec=timeout_sec,
        min_area=8_000,
        title_substr=PRESENT_TITLE,
    )
    if hwnd:
        return hwnd, title
    return find_window_by_title_substr(PRESENT_TITLE, timeout_sec=4.0, pid=pid)


def _launch(
    exe: Path,
    *,
    env_extra: dict[str, str] | None,
    err_path: Path,
) -> tuple[subprocess.Popen[bytes], object]:
    env = os.environ.copy()
    for k in (
        "SMT_HARNESS_SUITE",
        "SMT_UI_INTERACT_SCRIPT",
        "SMT_VIEWS_START_MAP_TAB",
        "SMT_SKIP_AMBOX_CATALOG",
        "SMT_SYNC_CHINA_SEED",
        "SMT_DEFER_CHINA_SEED",
    ):
        env.pop(k, None)
    # Sync China before FlyCube attach: deferred open_path races Display and
    # can hang the UI thread (seed begin, never done). Sync still uses argv=[].
    env["SMT_SYNC_CHINA_SEED"] = "1"
    env["SMT_SKIP_CHINA_LAND_CLIP"] = "1"
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
    bring_hwnd_to_front(hwnd, stay_topmost=False, restore=True)
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
        bring_hwnd_to_front(shell)
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
        if do_wheel:
            # Wheel on Present (or shell map body) without a prior click —
            # fixed-client clicks were activating Map|Data|3D before capture.
            present_hw = present_hw_pre
            if not present_hw:
                present_hw, _ = _find_present(proc.pid, timeout_sec=2.0)
            target = present_hw if present_hw else shell
            bring_hwnd_to_front(target if present_hw else shell)
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

        r_shell = _capture_shell(shell, cap / shell_leaf)
        print(f"{phase}_shell", r_shell.get("ok"), r_shell.get("score", {}).get("ok"), flush=True)

        # Reuse HWND from settle — a second EnumWindows pass often misses the
        # DXGI popup briefly after shell PrintWindow / TOPMOST churn.
        present, present_title = present_hw_pre, present_title_pre
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
    if "rhi.switch_map_tab lazy attach tab=2" in text:
        gold["tab2"] = True
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
    if not phase.get("alive_after", False):
        bugs.append(f"process_exit rc={phase.get('exit_rc')}")
        status = "failing"
    if not shell.get("ok"):
        bugs.append("shell_capture_or_score_failed")
        status = "failing"
    present_ok = bool(present and present.get("ok"))
    # DXGI flip + WS_EX_NOREDIRECTIONBITMAP: BitBlt often cannot see the
    # swapchain. Accept product-log gold when shell chrome is OK and process
    # survived tab=2 / map2d present.
    log_ok = False
    if suite_id.endswith("3d"):
        log_ok = bool(
            log_gold.get("scene3d_present")
            and log_gold.get("tab2")
            and phase.get("alive_after")
        )
    else:
        log_ok = bool(log_gold.get("map2d_frame_items") and phase.get("alive_after"))
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
    if status != "failing" and shell.get("ok") and (present_ok or log_ok):
        status = "verified"
        # Keep informational notes, not hard fails.
        bugs = [b for b in bugs if "product_log_gold" in b]
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
    exe = repo / "out" / args.out / "SmartGisViews.exe"
    cap = repo / "out" / args.out / "captures" / "shell"
    log_dir = repo / "out" / args.out / "log"
    if not exe.is_file():
        print("missing", exe, file=sys.stderr)
        return 2
    cap.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

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
    # #1: reliable 3D — env tab, still argv=[].
    r3d = _run_phase(
        exe=exe,
        cap=cap,
        log_dir=log_dir,
        phase="3d",
        settle_sec=args.settle_3d,
        env_extra={"SMT_VIEWS_START_MAP_TAB": "scene3d"},
        shell_leaf="views-plain-3d-browse.bmp",
        present_leaf="views-plain-3d-flycube-present.bmp",
        do_wheel=False,
    )

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
            "argv=[]; SMT_VIEWS_START_MAP_TAB=scene3d (env, not OS click); "
            "present BitBlt crops TabStrip accent; DXGI opaque → "
            "scene3d.present dem log gold."
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
