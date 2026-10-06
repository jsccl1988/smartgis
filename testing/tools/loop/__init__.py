# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Shared outer-loop helpers for SmartGisViews suite runners.

Layers:
  contract/ — suite.json types + loader
  drive/    — kill / build / env / inproc / OS inject / plugin.json
  interact/ — DSL parse + OS inject
  record/   — HWND capture / ffmpeg / IL record
  score/    — BMP / marks / extra gates (round.py)
  review/   — inspect PNG + visual_review emit
  runner.py — round loop (compose the layers)
"""

from .gate import GATE_SUITE_IDS, run_product_gate
from .contract import Suite, list_suite_ids, load_suite
from .runner import run_suite

__all__ = [
    "GATE_SUITE_IDS",
    "Suite",
    "load_suite",
    "list_suite_ids",
    "run_product_gate",
    "run_suite",
]
