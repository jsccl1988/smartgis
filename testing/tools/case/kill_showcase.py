#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Compat shim — prefer loop.kill / loop_runner kill path."""

from __future__ import annotations

import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[1]  # testing/tools
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.kill import kill_pid_tree, kill_showcase_apps  # noqa: E402

__all__ = ["kill_showcase_apps", "kill_pid_tree"]
