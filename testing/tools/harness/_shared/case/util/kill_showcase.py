#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Compat shim — prefer loop.kill / loop_runner kill path."""

from __future__ import annotations

import sys
from pathlib import Path


def _tools_dir() -> Path:
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "loop_runner.py").is_file():
            return p
        p = p.parent
    raise RuntimeError("testing/tools (loop_runner.py) not found")


_TOOLS = _tools_dir()
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.kill import kill_pid_tree, kill_showcase_apps  # noqa: E402

__all__ = ["kill_showcase_apps", "kill_pid_tree"]
