#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run a SmartGisViews harness suite until green (or max rounds).

Suite contracts live under testing/tools/suites/*.json and share ids with
the in-process C++ ScenarioRegistry (browse / input / console / ...).

  py -3 testing/tools/loop_runner.py --suite browse
  py -3 testing/tools/loop_runner.py --suite input --no-build
  py -3 testing/tools/loop_runner.py --list
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop import list_suite_ids, load_suite, run_suite  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", help="suite id (see --list)")
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
    args = parser.parse_args(argv)

    if args.list:
        for sid in list_suite_ids():
            print(sid)
        return 0
    if not args.suite:
        parser.error("--suite is required (or pass --list)")

    suite = load_suite(args.suite)
    return run_suite(
        suite,
        no_build=args.no_build,
        rounds=args.rounds,
        timeout_sec=args.timeout,
        config=args.out,
        bmp_only=args.bmp,
    )


if __name__ == "__main__":
    sys.exit(main())
