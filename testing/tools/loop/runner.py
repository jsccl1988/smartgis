# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Outer-loop: kill -> build -> run -> score marks/BMP -> retry."""

from __future__ import annotations

import json
import time
from pathlib import Path

from .contract import ROOT, Suite
from .drive.build import build_debug
from .drive.execute import execute_round
from .drive.prep import kill_round, unlink_stale_probes, wait_exe_ready
from .review.emit_review import maybe_emit_visual_review
from .score.bmp import score_bmp
from .score.marks import read_mark_text
from .score.round import _score_round, attach_runtime_gates


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
            maybe_emit_visual_review(
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
        kill_round(suite)
        if not no_build:
            if build_debug(ROOT, suite.build_target) != 0:
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
        if not wait_exe_ready(suite, config=config):
            last = {"ok": False, "round": round_i, "error": "exe_busy"}
            report.write_text(json.dumps(last, indent=2), encoding="utf-8")
            time.sleep(2.0)
            continue

        unlink_stale_probes(
            suite,
            mark=mark,
            bmp=bmp,
            review_prep=review_prep,
            force_run=force_run,
        )

        started = time.time()
        executed = execute_round(
            suite,
            exe=exe,
            out=out,
            timeout=timeout,
            bmp_path=bmp,
            captures_root=captures_root,
        )
        mark_text = read_mark_text(mark)
        last = _score_round(
            suite,
            rc=executed.rc,
            mark_text=mark_text,
            bmp_path=bmp,
            started=started,
        )
        last["round"] = round_i
        last["driver"] = suite.driver
        last["t_ms"] = int((time.time() - started) * 1000)
        last["steps"] = [
            {"op": "suite_start", "t_ms": 0},
            {"op": "suite_end", "t_ms": last["t_ms"]},
        ]
        attach_runtime_gates(
            suite,
            last,
            inject_report=executed.inject_report,
            record_report=executed.record_report,
            env=executed.env,
            captures_root=captures_root,
        )
        if bmp is not None and bmp.exists():
            last["bmp_age_s"] = round(time.time() - bmp.stat().st_mtime, 3)
            if review_prep or suite.is_reviewable():
                maybe_emit_visual_review(
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
