# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""CLI for loop_runner: suite loop, review-prep, and IL record."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import list_suite_ids, load_suite, run_suite
from .gate import GATE_SUITE_IDS, run_product_gate


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run a SmartGisViews harness suite until green (or max rounds)."
        )
    )
    parser.add_argument("--suite", help="suite id (see --list)")
    parser.add_argument(
        "--gate",
        action="store_true",
        help="product runtime gate: gpu PE --self-test then SmartGIS --harness",
    )
    parser.add_argument("--list", action="store_true", help="list suite ids")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--rounds", type=int, default=None)
    parser.add_argument("--timeout", type=int, default=None)
    parser.add_argument(
        "--out",
        choices=("Debug", "Release"),
        default="Debug",
        help="out/<config> root (default Debug)",
    )
    parser.add_argument(
        "--bmp",
        type=Path,
        default=None,
        help="Score an existing BMP only (skip build/run; suite must define bmp)",
    )
    parser.add_argument(
        "--review-prep",
        action="store_true",
        help=(
            "One-shot visual review prep: write *.inspect.png + *_visual_review.json "
            "(reuse existing BMP unless --force-run); does not auto-fix bugs"
        ),
    )
    parser.add_argument(
        "--force-run",
        action="store_true",
        help="With --review-prep: re-exec showcase instead of reusing captures BMP",
    )
    parser.add_argument(
        "--record-il",
        action="store_true",
        help="Record interactions to Interact DSL (.il); see loop/record/il_recorder.py",
    )
    parser.add_argument(
        "--attach",
        action="store_true",
        help="With --record-il: attach to existing window (do not launch)",
    )
    parser.add_argument(
        "--title",
        default="SmartGIS Views",
        help="With --record-il: window title substring",
    )
    parser.add_argument(
        "--exe",
        default=None,
        help="With --record-il: exe to launch (default out/<config>/SmartGIS.exe)",
    )
    parser.add_argument(
        "--il-out",
        type=Path,
        default=None,
        help="With --record-il: output dir (default out/<config>/captures/record)",
    )
    parser.add_argument(
        "--il-name",
        default="recorded",
        help="With --record-il: script name / .il stem",
    )
    parser.add_argument(
        "--video",
        action="store_true",
        help="With --record-il: also capture HWND video/BMP",
    )
    parser.add_argument(
        "--no-agent",
        action="store_true",
        help="With --record-il: skip DebugAgent semantic poll",
    )
    args, launch_args = parser.parse_known_args(argv)

    if args.record_il:
        from .record.il_recorder import (
            _default_captures,
            _resolve_exe,
            run_record_session,
        )

        exe_path = None if args.attach else _resolve_exe(args.exe, args.out)
        if not args.attach and (exe_path is None or not exe_path.is_file()):
            print(
                f"error: exe not found ({args.exe!r} / out/{args.out})",
                flush=True,
            )
            return 2
        out_dir = args.il_out or _default_captures(args.out)
        report = run_record_session(
            title=args.title,
            exe=exe_path,
            attach=args.attach,
            out_dir=out_dir,
            script_name=args.il_name,
            also_video=args.video,
            agent=not args.no_agent,
            launch_args=list(launch_args),
        )
        return 0 if report.get("ok") else 1

    if args.list:
        for sid in list_suite_ids():
            print(sid)
        return 0
    if args.gate:
        print("gate suites: " + ", ".join(GATE_SUITE_IDS), flush=True)
        return run_product_gate(
            no_build=args.no_build,
            rounds=args.rounds if args.rounds is not None else 1,
            timeout_sec=args.timeout,
            config=args.out,
        )
    if not args.suite:
        parser.error("--suite is required (or pass --list / --gate / --record-il)")

    suite = load_suite(args.suite)
    return run_suite(
        suite,
        no_build=args.no_build,
        rounds=args.rounds,
        timeout_sec=args.timeout,
        config=args.out,
        bmp_only=args.bmp,
        review_prep=args.review_prep,
        force_run=args.force_run,
    )


if __name__ == "__main__":
    sys.exit(main())
