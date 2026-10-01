# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Outer-loop: kill -> build -> run -> score marks/BMP -> retry."""

from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

from . import build as build_mod
from . import process as process_mod
from .record.hwnd import HwndRecorder, record_enabled
from .review.emit_review import emit_visual_review
from .score.bmp import score_bmp
from .score.bmp_io import pixel_diff_frac
from .score.marks import read_mark_text, score_marks
from .suite import ROOT, Suite


def _maybe_emit_visual_review(
    suite: Suite,
    *,
    config: str,
    result: dict,
    bmp_path: Path | None,
    force: bool = False,
) -> Path | None:
    """Write inspect PNG + pending review JSON when suite is reviewable."""
    if not force and not suite.is_reviewable():
        return None
    if suite.bmp is None:
        return None
    score = result.get("bmp") if isinstance(result.get("bmp"), dict) else None
    if score is None and isinstance(result.get("gates"), dict) and "ok" in result:
        # --bmp / review-prep reuse path: score_bmp dict is the result itself.
        score = {
            k: v
            for k, v in result.items()
            if k
            not in (
                "suite",
                "visual_review",
                "round",
                "driver",
                "t_ms",
                "steps",
                "os_inject",
                "record",
                "record_path",
                "bmp_age_s",
                "marks",
                "missing",
                "error",
                "rc_forgiven",
                "showcase_rc",
            )
        }
    try:
        path = emit_visual_review(
            suite,
            config=config,
            score=score,
            status="pending",
            bmp_path=bmp_path,
            write_inspect=True,
        )
    except Exception as exc:  # noqa: BLE001
        print(f"warn: visual_review emit failed ({exc})", flush=True)
        return None
    result["visual_review"] = {
        "path": str(path),
        "status": "pending",
        "inspect_png": (bmp_path.stem + ".inspect.png") if bmp_path else "",
    }
    print(f"visual_review: {path}", flush=True)
    return path


def _kill(suite: Suite) -> None:
    if suite.kill_showcase:
        try:
            from .kill import kill_showcase_apps

            # 4s: OpenGL ICD needs settle after TerminateProcess / taskkill
            # or the next scene3d round exits early with empty marks / bmp_missing.
            kill_showcase_apps(settle_sec=4.0)
        except Exception as exc:  # noqa: BLE001
            print(f"warn: kill_showcase failed ({exc}); falling back", flush=True)
    # Always clear the suite image — kill_showcase only matches marker argv;
    # bare SmartGis.exe leftovers race OS-inject HWND bind / DelayInit.
    process_mod.kill_exe(suite.exe_name)


def _wait_exe_ready(suite: Suite, *, config: str) -> bool:
    try:
        from .kill import wait_exe_ready

        exe = suite.exe_path(config)
        if not exe.is_file():
            return False
        ok = wait_exe_ready(exe, timeout_sec=90.0)
        if not ok:
            print(f"warn: exe not ready within timeout: {exe}", flush=True)
        return ok
    except Exception as exc:  # noqa: BLE001
        print(f"warn: wait_exe_ready failed ({exc})", flush=True)
        return suite.exe_path(config).is_file()


def _score_zoom_gate(
    suite: Suite,
    *,
    captures_root: Path,
    inject_report: dict,
) -> dict:
    """Compare zoom_before/after BMPs; fail when pixel_diff_frac is too low."""
    assert suite.zoom_gate is not None
    zg = suite.zoom_gate
    before = captures_root / zg.before.replace("\\", "/")
    after = captures_root / zg.after.replace("\\", "/")
    cap = inject_report.get("zoom_capture") if isinstance(inject_report, dict) else None
    out: dict = {
        "before": str(before),
        "after": str(after),
        "min_pixel_diff_frac": zg.min_pixel_diff_frac,
        "thresh": zg.thresh,
        "ok": False,
    }
    if isinstance(cap, dict):
        out["capture"] = cap
    if not before.is_file() or not after.is_file():
        out["error"] = "zoom_bmp_missing"
        return out
    try:
        frac = pixel_diff_frac(before, after, thresh=zg.thresh)
    except Exception as exc:  # noqa: BLE001
        out["error"] = f"pixel_diff: {exc}"
        return out
    out["pixel_diff_frac"] = round(float(frac), 6)
    out["ok"] = float(frac) >= float(zg.min_pixel_diff_frac)
    return out


def _score_round(
    suite: Suite,
    *,
    rc: int,
    mark_text: str,
    bmp_path: Path | None,
    started: float,
) -> dict:
    want_marks = "marks" in suite.probes or bool(suite.required_marks)
    want_bmp = "bmp" in suite.probes or suite.bmp is not None

    result: dict = {
        "suite": suite.id,
        "showcase_rc": rc,
        "ok": True,
        "gates": {},
    }

    effective_rc = rc
    if (
        suite.bmp is not None
        and suite.bmp.accept_nonzero_rc_if_bmp
        and rc != 0
        and bmp_path is not None
        and bmp_path.exists()
        and bmp_path.stat().st_size > suite.bmp.min_bytes
    ):
        print(
            f"showcase rc={rc} but BMP present — treating exit as 0 for gates",
            flush=True,
        )
        effective_rc = 0
        result["rc_forgiven"] = True
    if (
        effective_rc != 0
        and suite.accept_nonzero_rc_if_marks
        and suite.required_marks
    ):
        landed = {line.strip() for line in mark_text.splitlines() if line.strip()}
        if all(m in landed for m in suite.required_marks):
            print(
                f"showcase rc={rc} but required marks present — "
                "treating exit as 0 for gates",
                flush=True,
            )
            effective_rc = 0
            result["rc_forgiven"] = True

    if want_marks:
        marks = score_marks(effective_rc, mark_text, suite.required_marks)
        result["marks"] = marks.get("marks")
        result["missing"] = marks.get("missing")
        result["gates"].update(marks.get("gates") or {})
        if not marks["ok"]:
            result["ok"] = False
    else:
        result["gates"]["exit==0"] = effective_rc == 0
        if effective_rc != 0:
            result["ok"] = False

    if want_bmp:
        assert suite.bmp is not None
        if bmp_path is None or not bmp_path.exists():
            result["ok"] = False
            result["error"] = "bmp_missing"
            result["gates"]["bmp_present"] = False
        elif bmp_path.stat().st_mtime < (started - 0.5):
            result["ok"] = False
            result["error"] = "bmp_stale"
            result["gates"]["bmp_present"] = False
        else:
            bmp_score = score_bmp(bmp_path, suite.bmp.score_id)
            result["bmp"] = bmp_score
            result["gates"].update(
                {f"bmp:{k}": v for k, v in (bmp_score.get("gates") or {}).items()}
            )
            result["gates"]["bmp_ok"] = bool(bmp_score.get("ok"))
            if not bmp_score.get("ok"):
                if suite.bmp.soft:
                    result["gates"]["bmp_ok_soft"] = False
                    result["bmp_soft_fail"] = True
                else:
                    result["ok"] = False
            if want_marks is False and effective_rc != 0:
                result["ok"] = False

    return result


def _prepare_env(suite: Suite) -> dict[str, str]:
    env = process_mod.merge_env(suite.env)
    env["SMT_HARNESS_SUITE"] = suite.id
    # Parent shells often leave SMT_FORCE_GDI_* set from prior self-test /
    # map2d runs; that forces views-scene3d.gdi and breaks 3D suites.
    for key in (
        "SMT_FORCE_GDI_MAP_OVERLAY",
        "SMT_FORCE_CONTENT_MAPVIEW_2D",
        "SMT_PREFER_FLYCUBE_2D",
        "SMT_PREFER_GDI_DEVICE",
    ):
        env.pop(key, None)
    if suite.id in ("browse.3d", "ui.scene", "browse"):
        env["SMT_FORCE_CONTENT_MAPVIEW_2D"] = "0"
        env["SMT_PREFER_FLYCUBE_2D"] = "1"
        env["SMT_FORCE_GDI_MAP_OVERLAY"] = "0"
    script = suite.script_path()
    if script is not None and script.is_file():
        env["SMT_UI_INTERACT_SCRIPT"] = str(script.resolve())
    if suite.driver == "os":
        env["SMT_UI_INTERACT_DRIVER"] = "os"
        env.setdefault("SMT_UI_INTERACT_OS_WAIT_MS", "8000")
        # Capture after OS inject window; linger not needed.
        env.setdefault("SMT_UI_SHOWCASE_LINGER_MS", "0")
    return env


def _run_inproc_process(
    suite: Suite, *, exe: Path, out: Path, env: dict[str, str], timeout: int
) -> int:
    cmd = [str(exe), *suite.argv]
    return process_mod.run_process(
        cmd,
        cwd=out,
        env=env,
        timeout_sec=timeout,
        kill_image=suite.exe_name,
    )


def _attach_recorder(
    suite: Suite, *, env: dict[str, str], captures: Path
) -> HwndRecorder | None:
    if not record_enabled(env):
        return None
    fps = 10.0
    raw_fps = str(env.get("SMT_HARNESS_RECORD_FPS", "")).strip()
    if raw_fps:
        try:
            fps = float(raw_fps)
        except ValueError:
            fps = 10.0
    rec = HwndRecorder(
        captures_dir=captures,
        suite_id=suite.id,
        title_substr=suite.window_title,
        fps=fps,
        find_timeout_sec=min(45.0, float(suite.timeout_sec)),
    )
    return rec


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
        capture_hwnd_bmp_ex,
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
        if wait_bmp and bmp_path is not None:
            min_bytes = suite.bmp.min_bytes if suite.bmp is not None else 1000
            deadline = time.time() + min(75.0, float(timeout))
            print(f"os-inject wait_bmp={bmp_path}", flush=True)
            while time.time() < deadline:
                if bmp_path.exists() and bmp_path.stat().st_size > min_bytes:
                    print("os-inject bmp ready", flush=True)
                    break
                if proc.poll() is not None:
                    break
                time.sleep(0.25)

        # Bind HWND to the launched PE only — title substr alone matches Cursor
        # tabs like ``legacy.browse.2d.il - smartgis - Cursor``. Also skip
        # short-lived splash HWNDs titled exactly ``SmartGis``.
        child_pid = int(proc.pid)
        hwnd, win_title = wait_stable_shell_hwnd(
            child_pid,
            title_substr=suite.window_title,
            timeout_sec=min(45.0, float(timeout)),
            min_area=200_000,
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
            map_hwnd, map_how = find_map_client_hwnd(
                int(hwnd), timeout_sec=min(25.0, float(timeout))
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
            if not is_window(int(hwnd)):
                hwnd2, title2 = wait_stable_shell_hwnd(
                    child_pid,
                    title_substr=suite.window_title,
                    timeout_sec=min(20.0, float(timeout)),
                    min_area=200_000,
                    stable_ms=400,
                )
                if hwnd2:
                    hwnd, win_title = hwnd2, title2 or win_title
                    map_hwnd, map_how = find_map_client_hwnd(
                        int(hwnd), timeout_sec=min(15.0, float(timeout))
                    )
                    inject_hwnd = int(map_hwnd) if map_hwnd else int(hwnd)
            bring_hwnd_to_front(int(hwnd))
            step_t0 = time.time()
            zoom_cfg = None
            if suite.zoom_gate is not None:
                zoom_cfg = {
                    "before": suite.zoom_gate.before,
                    "after": suite.zoom_gate.after,
                    "settle_ms": suite.zoom_gate.settle_ms,
                }
            inject_report = run_script(
                script,
                inject_hwnd,
                inject_default=suite.os_inject_default,
                captures_root=captures_root,
                zoom_gate=zoom_cfg,
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
                # PrintWindow avoids IDE occlusion of the HWND rect.
                time.sleep(0.4)
                bring_hwnd_to_front(int(hwnd))
                time.sleep(0.2)
                cap_hwnd = int(hwnd)
                ok, frac = capture_hwnd_bmp_ex(
                    cap_hwnd, Path(bmp_path), prefer_printwindow=True
                )
                if not ok or frac > 0.90:
                    time.sleep(0.5)
                    bring_hwnd_to_front(cap_hwnd)
                    ok2, frac2 = capture_hwnd_bmp_ex(
                        cap_hwnd, Path(bmp_path), prefer_printwindow=True
                    )
                    if ok2 and frac2 < frac:
                        ok, frac = ok2, frac2
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
                    "method": "prefer_printwindow",
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


def run_suite(
    suite: Suite,
    *,
    no_build: bool = False,
    rounds: int | None = None,
    timeout_sec: int | None = None,
    config: str = "Debug",
    bmp_only: Path | None = None,
    review_prep: bool = False,
    force_run: bool = False,
) -> int:
    out = suite.out_dir(config)
    out.mkdir(parents=True, exist_ok=True)
    captures = suite.captures_dir(config)
    captures.mkdir(parents=True, exist_ok=True)
    captures_root = suite.captures_root(config)
    captures_root.mkdir(parents=True, exist_ok=True)
    report = suite.report_path(config)
    report.parent.mkdir(parents=True, exist_ok=True)

    if bmp_only is not None:
        if suite.bmp is None:
            print(f"suite {suite.id} has no bmp probe", flush=True)
            return 2
        path = bmp_only.resolve()
        if not path.exists():
            print(f"missing BMP: {path}", flush=True)
            return 2
        last = score_bmp(path, suite.bmp.score_id)
        last["suite"] = suite.id
        if review_prep or suite.is_reviewable():
            _maybe_emit_visual_review(
                suite, config=config, result=last, bmp_path=path, force=review_prep
            )
        report.write_text(json.dumps(last, indent=2), encoding="utf-8")
        print(json.dumps(last, indent=2), flush=True)
        if review_prep:
            print(
                f"REVIEW-PREP {suite.id}: inspect + visual_review written "
                "(bugs still pending human confirm)",
                flush=True,
            )
            return 0 if path.exists() else 1
        if last.get("ok"):
            print(f"PASS {suite.id} visual gates", flush=True)
            return 0
        print(f"FAIL {suite.id} visual gates", flush=True)
        return 1

    # --review-prep with existing capture: skip exe if BMP already present.
    if review_prep and not force_run and suite.bmp is not None:
        existing = suite.bmp_path(config)
        if (
            existing is not None
            and existing.is_file()
            and existing.stat().st_size > suite.bmp.min_bytes
        ):
            print(
                f"REVIEW-PREP: reuse existing BMP {existing} "
                "(pass --force-run to re-exec)",
                flush=True,
            )
            return run_suite(
                suite,
                no_build=True,
                rounds=1,
                timeout_sec=timeout_sec,
                config=config,
                bmp_only=existing,
                review_prep=True,
            )

    exe = suite.exe_path(config)
    mark = suite.mark_path(config)
    bmp = suite.bmp_path(config)
    n_rounds = max(1, rounds if rounds is not None else suite.rounds)
    if review_prep:
        n_rounds = 1
    timeout = timeout_sec if timeout_sec is not None else suite.timeout_sec
    last: dict | None = None

    for round_i in range(1, n_rounds + 1):
        print(f"\n=== {suite.id} loop round {round_i}/{n_rounds} ===", flush=True)
        _kill(suite)
        if not no_build:
            if build_mod.build_debug(ROOT, suite.build_target) != 0:
                print("BUILD failed — retry next round", flush=True)
                last = {"ok": False, "round": round_i, "error": "build_failed"}
                report.write_text(json.dumps(last, indent=2), encoding="utf-8")
                time.sleep(2.0)
                continue
        if not exe.exists():
            print(f"missing exe: {exe}", flush=True)
            last = {"ok": False, "round": round_i, "error": "missing_exe"}
            report.write_text(json.dumps(last, indent=2), encoding="utf-8")
            time.sleep(2.0)
            continue
        if not _wait_exe_ready(suite, config=config):
            last = {"ok": False, "round": round_i, "error": "exe_busy"}
            report.write_text(json.dumps(last, indent=2), encoding="utf-8")
            time.sleep(2.0)
            continue

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

        env = _prepare_env(suite)
        started = time.time()
        inject_report: dict | None = None
        record_report: dict | None = None
        # Record stays at captures/record/ (not under scenario family).
        recorder = _attach_recorder(suite, env=env, captures=captures_root)
        if suite.driver == "os":
            rc, inject_report, record_report = _run_os_process(
                suite,
                exe=exe,
                out=out,
                env=env,
                timeout=timeout,
                recorder=recorder,
                bmp_path=bmp,
                captures_root=captures_root,
            )
        else:
            # Inproc: start record once HWND exists (poll title while exe runs).
            if recorder is not None:
                import threading

                def _bg_record() -> None:
                    nonlocal record_report
                    record_report = recorder.start_after_hwnd(wait_for_hwnd=True)

                t = threading.Thread(target=_bg_record, daemon=True)
                t.start()
            try:
                rc = _run_inproc_process(
                    suite, exe=exe, out=out, env=env, timeout=timeout
                )
            finally:
                if recorder is not None:
                    record_report = recorder.stop()

        mark_text = read_mark_text(mark)
        last = _score_round(
            suite, rc=rc, mark_text=mark_text, bmp_path=bmp, started=started
        )
        last["round"] = round_i
        last["driver"] = suite.driver
        last["t_ms"] = int((time.time() - started) * 1000)
        last["steps"] = [
            {"op": "suite_start", "t_ms": 0},
            {"op": "suite_end", "t_ms": last["t_ms"]},
        ]
        if inject_report is not None:
            last["os_inject"] = inject_report
            last["gates"]["os_inject_ok"] = bool(inject_report.get("ok"))
            if not inject_report.get("ok"):
                last["ok"] = False
            if "t_ms" in inject_report:
                last["steps"].insert(
                    1,
                    {
                        "op": "os_inject",
                        "t_ms": int(inject_report.get("t_ms") or 0),
                        "ok": bool(inject_report.get("ok")),
                    },
                )
            if suite.zoom_gate is not None:
                # Do not score leftover before/after BMPs when this round never
                # captured zoom sidecars (hwnd_timeout / crash before inject).
                zoom_cap = (
                    inject_report.get("zoom_capture")
                    if isinstance(inject_report, dict)
                    else None
                )
                if not isinstance(zoom_cap, dict):
                    zg = {
                        "ok": False,
                        "error": "zoom_not_captured_this_run",
                        "min_pixel_diff_frac": suite.zoom_gate.min_pixel_diff_frac,
                        "thresh": suite.zoom_gate.thresh,
                    }
                else:
                    zg = _score_zoom_gate(
                        suite,
                        captures_root=captures_root,
                        inject_report=inject_report,
                    )
                last["zoom_gate"] = zg
                last["gates"]["zoom_pixel_diff"] = bool(zg.get("ok"))
                if not zg.get("ok"):
                    last["ok"] = False
                    print(
                        f"FAIL {suite.id} zoom_gate "
                        f"{zg.get('error') or ('pixel_diff_frac=' + str(zg.get('pixel_diff_frac')))} "
                        f"(min={suite.zoom_gate.min_pixel_diff_frac})",
                        flush=True,
                    )
                else:
                    print(
                        f"PASS {suite.id} zoom_gate pixel_diff_frac="
                        f"{zg.get('pixel_diff_frac')}",
                        flush=True,
                    )
        if record_report is not None:
            last["record"] = record_report
            if record_report.get("record_path"):
                last["record_path"] = record_report["record_path"]
            # Recording is optional: missing ffmpeg / hwnd skip must not fail gates.
            last["gates"]["record_soft_ok"] = record_report.get("mode") != "none"
        if bmp is not None and bmp.exists():
            last["bmp_age_s"] = round(time.time() - bmp.stat().st_mtime, 3)
            if review_prep or suite.is_reviewable():
                _maybe_emit_visual_review(
                    suite,
                    config=config,
                    result=last,
                    bmp_path=bmp,
                    force=review_prep,
                )
        report.write_text(json.dumps(last, indent=2), encoding="utf-8")
        print(json.dumps(last, indent=2), flush=True)
        if review_prep:
            print(
                f"REVIEW-PREP {suite.id}: inspect + visual_review written "
                "(bugs still pending human confirm)",
                flush=True,
            )
            # Prep succeeds when BMP/artifacts exist; score fail is informational.
            if bmp is not None and bmp.exists():
                return 0
            return 1 if not last.get("ok") else 0
        if last["ok"]:
            print(f"PASS {suite.id} gates", flush=True)
            return 0
        print(f"FAIL {suite.id} gates — rebuild/retry", flush=True)
        time.sleep(1.5)

    print(f"STOPPED without {suite.id} PASS", flush=True)
    return 1
