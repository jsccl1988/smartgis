# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Outer-loop: kill -> build -> run -> score marks/BMP -> retry."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

from . import build as build_mod
from . import process as process_mod
from .score_bmp import score_bmp
from .score_marks import read_mark_text, score_marks
from .suite import ROOT, TOOLS_DIR, Suite


def _kill(suite: Suite) -> None:
    if suite.kill_showcase:
        try:
            from .kill import kill_showcase_apps

            kill_showcase_apps(settle_sec=0.5)
            return
        except Exception as exc:  # noqa: BLE001
            print(f"warn: kill_showcase failed ({exc}); falling back", flush=True)
    process_mod.kill_exe(suite.exe_name)


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
                result["ok"] = False
            if want_marks is False and effective_rc != 0:
                result["ok"] = False

    return result


def run_suite(
    suite: Suite,
    *,
    no_build: bool = False,
    rounds: int | None = None,
    timeout_sec: int | None = None,
    config: str = "Debug",
    bmp_only: Path | None = None,
) -> int:
    out = suite.out_dir(config)
    out.mkdir(parents=True, exist_ok=True)
    report = suite.report_path(config)

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
        report.write_text(json.dumps(last, indent=2), encoding="utf-8")
        print(json.dumps(last, indent=2), flush=True)
        if last.get("ok"):
            print(f"PASS {suite.id} visual gates", flush=True)
            return 0
        print(f"FAIL {suite.id} visual gates", flush=True)
        return 1

    exe = suite.exe_path(config)
    mark = suite.mark_path(config)
    bmp = suite.bmp_path(config)
    n_rounds = max(1, rounds if rounds is not None else suite.rounds)
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

        if mark is not None and mark.exists():
            try:
                mark.unlink()
            except OSError:
                pass
        if bmp is not None and bmp.exists():
            try:
                bmp.unlink()
            except OSError as exc:
                print(f"warn: could not delete stale BMP: {exc}", flush=True)

        env = process_mod.merge_env(suite.env)
        cmd = [str(exe), *suite.argv]
        started = time.time()
        rc = process_mod.run_process(
            cmd,
            cwd=out,
            env=env,
            timeout_sec=timeout,
            kill_image=suite.exe_name,
        )
        mark_text = read_mark_text(mark)
        last = _score_round(
            suite, rc=rc, mark_text=mark_text, bmp_path=bmp, started=started
        )
        last["round"] = round_i
        if bmp is not None and bmp.exists():
            last["bmp_age_s"] = round(time.time() - bmp.stat().st_mtime, 3)
        report.write_text(json.dumps(last, indent=2), encoding="utf-8")
        print(json.dumps(last, indent=2), flush=True)
        if last["ok"]:
            print(f"PASS {suite.id} gates", flush=True)
            return 0
        print(f"FAIL {suite.id} gates — rebuild/retry", flush=True)
        time.sleep(0.5)

    print(f"STOPPED without {suite.id} PASS", flush=True)
    return 1
