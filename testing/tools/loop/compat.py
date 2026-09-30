# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Compat entrypoints: old *.py script names -> suite id."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .runner import run_suite
from .suite import load_suite

# Basename of legacy scripts -> suites/<id>.json
SCRIPT_ALIASES: dict[str, str] = {
    "browse_loop.py": "browse",
    "input_loop.py": "input",
    "ui_shot_loop.py": "ui.shell",
    "map2d_shot_loop.py": "map2d.china",
    "scene3d_shot_loop.py": "atmosphere.full",
    "orthogrid_shot_loop.py": "map2d.orthogrid",
    "legacy_map2d_shot_loop.py": "legacy.map2d.china",
    "legacy_scene3d_shot_loop.py": "legacy.scene3d.china",
    "pointcloud_load_loop.py": "pointcloud.load",
}


def resolve_suite_id(script_file: str, argv: list[str]) -> tuple[str, list[str]]:
    name = Path(script_file).name
    suite_id = SCRIPT_ALIASES.get(name)
    if not suite_id:
        raise KeyError(
            f"no suite alias for {name}; known: {', '.join(sorted(SCRIPT_ALIASES))}"
        )
    rest = list(argv)
    if name == "legacy_scene3d_shot_loop.py" and "--d3d" in rest:
        suite_id = "legacy.scene3d.china.d3d"
        rest = [a for a in rest if a != "--d3d"]
    return suite_id, rest


def main_for_script(script_file: str, argv: list[str] | None = None) -> int:
    """CLI shared by thin wrappers under testing/tools/case/*.py."""
    raw = list(sys.argv[1:] if argv is None else argv)
    suite_id, rest = resolve_suite_id(script_file, raw)

    parser = argparse.ArgumentParser(
        description=f"Thin wrapper -> loop_runner --suite {suite_id}"
    )
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--rounds", type=int, default=None)
    parser.add_argument("--timeout", type=int, default=None)
    parser.add_argument("--bmp", type=Path, default=None)
    parser.add_argument(
        "--out",
        choices=("Debug", "Release"),
        default="Debug",
    )
    args = parser.parse_args(rest)
    return run_suite(
        load_suite(suite_id),
        no_build=args.no_build,
        rounds=args.rounds,
        timeout_sec=args.timeout,
        config=args.out,
        bmp_only=args.bmp,
    )
