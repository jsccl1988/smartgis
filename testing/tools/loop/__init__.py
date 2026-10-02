# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Shared outer-loop helpers for SmartGisViews suite runners.

Layers:
  suite.py / runner.py / gates.py / os_drive.py — suite contract + loop
  interact/ — DSL parse + OS inject (input_api / steps / os_inject)
  record/  — HWND capture / ffmpeg / IL record (see record/__init__.py)
  score/   — BMP / marks gates (bmp.py dispatches family modules)
  review/  — inspect PNG + visual_review emit
"""

from .suite import Suite, list_suite_ids, load_suite
from .runner import run_suite

__all__ = ["Suite", "load_suite", "list_suite_ids", "run_suite"]
