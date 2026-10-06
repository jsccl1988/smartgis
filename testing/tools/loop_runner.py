#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Run a SmartGisViews harness suite until green (or max rounds).

Suite contracts live under testing/tools/harness/<family>/<suite_id>/suite.json
and share ids with the in-process C++ ScenarioRegistry (browse / input / ...).

  py -3 testing/tools/loop_runner.py --suite browse
  py -3 testing/tools/loop_runner.py --suite input --no-build
  py -3 testing/tools/loop_runner.py --list
  py -3 testing/tools/loop_runner.py --gate --no-build
  py -3 testing/tools/loop_runner.py --record-il
  py -3 testing/tools/loop_runner.py --suite plugin.stormsurge --review-prep
  py -3 testing/tools/loop_runner.py --record-il --attach --title "SmartGIS Views"
"""

from __future__ import annotations

import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
