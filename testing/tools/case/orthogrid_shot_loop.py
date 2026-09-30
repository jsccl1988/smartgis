#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Thin alias -> loop_runner --suite <id> (see loop.compat.SCRIPT_ALIASES)."""

from __future__ import annotations

import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[1]  # testing/tools
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.compat import main_for_script  # noqa: E402

if __name__ == "__main__":
    sys.exit(main_for_script(__file__))
