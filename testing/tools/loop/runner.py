# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Outer-loop: kill -> build -> run -> score marks/BMP -> retry."""

from __future__ import annotations

import json
import time
from pathlib import Path

from . import build as build_mod
from . import process as process_mod
from .gates import (
    _score_click_gate,
    _score_motion_gate,
    _score_round,
    _score_zoom_gate,
)
from .os_drive import _run_os_process
from .record.hwnd import HwndRecorder, record_enabled
from .review.emit_review import emit_visual_review
from .score.bmp import score_bmp
from .score.marks import read_mark_text
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



def _prepare_env(suite: Suite) -> dict[str, str]:
    env = process_mod.merge_env(suite.env)
    env["SMT_HARNESS_SUITE"] = suite.id
    # Parent shells often leave SMT_FORCE_GDI_* set from prior self-test /
    # map2d runs; that forces views-scene3d.gdi and breaks 3D suites.
    # Also drop map2d matrix bench flags so map2d.china review-prep does not
    # inherit SMT_MAP2D_SHOWCASE_GPU=1 and crash before writing the BMP.
    for key in (
        "SMT_FORCE_GDI_MAP_OVERLAY",
        "SMT_FORCE_CONTENT_MAPVIEW_2D",
        "SMT_PREFER_FLYCUBE_2D",
        "SMT_PREFER_GDI_DEVICE",
        "SMT_MAP2D_SHOWCASE_GPU",
        "SMT_MAP2D_EXPORT_REUSE",
        "SMT_MAP2D_FPS_BENCH_MS",
    ):
        env.pop(key, None)
    # Suite.env wins (re-apply after scrub).
    for key, value in suite.env.items():
        env[str(key)] = str(value)
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
            if suite.click_gate is not None and suite.click_gate.enabled:
                cg = _score_click_gate(
                    suite,
                    captures_root=captures_root,
                    inject_report=inject_report
                    if isinstance(inject_report, dict)
                    else {},
                )
                last["click_gate"] = cg
                last["gates"]["click_ok"] = bool(cg.get("ok"))
                if not cg.get("ok"):
                    last["ok"] = False
                    print(
                        f"FAIL {suite.id} click_gate "
                        f"{cg.get('error') or cg}",
                        flush=True,
                    )
                else:
                    print(
                        f"PASS {suite.id} click_gate clicks={cg.get('clicks')} "
                        f"dblclicks={cg.get('dblclicks')}",
                        flush=True,
                    )
        if record_report is not None:
            last["record"] = record_report
            if record_report.get("record_path"):
                last["record_path"] = record_report["record_path"]
            # Recording is optional: missing ffmpeg / hwnd skip must not fail gates.
            last["gates"]["record_soft_ok"] = record_report.get("mode") != "none"
            # Hard motion gate only when this round actually recorded frames.
            if (
                suite.motion_gate is not None
                and record_enabled(env)
                and str(record_report.get("mode") or "").startswith(
                    ("bmp_burst", "ffmpeg")
                )
            ):
                mg = _score_motion_gate(suite, record_report=record_report)
                last["motion_gate"] = mg
                last["gates"]["motion_ok"] = bool(mg.get("ok"))
                if not mg.get("ok"):
                    last["ok"] = False
                    print(
                        f"FAIL {suite.id} motion_gate "
                        f"unique={mg.get('unique_frames')}/"
                        f"{mg.get('frame_count')} "
                        f"frac={mg.get('unique_frac')}",
                        flush=True,
                    )
                else:
                    print(
                        f"PASS {suite.id} motion_gate "
                        f"unique={mg.get('unique_frames')}/"
                        f"{mg.get('frame_count')} "
                        f"frac={mg.get('unique_frac')}",
                        flush=True,
                    )
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

