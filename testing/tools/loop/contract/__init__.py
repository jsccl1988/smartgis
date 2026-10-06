# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Suite JSON contract: types, capture paths, loader."""

from .load import find_suite_path, list_suite_ids, load_suite
from .model import (
    BmpProbe,
    ClickGate,
    FpsGate,
    MotionGate,
    Suite,
    VisualReview,
    ZoomGate,
)
from .paths import (
    HARNESS_DIR,
    ROOT,
    SHARED_DIR,
    TOOLS_DIR,
    capture_scenario_for_leaf,
    with_capture_scenario,
)

__all__ = [
    "BmpProbe",
    "ClickGate",
    "FpsGate",
    "HARNESS_DIR",
    "MotionGate",
    "ROOT",
    "SHARED_DIR",
    "Suite",
    "TOOLS_DIR",
    "VisualReview",
    "ZoomGate",
    "capture_scenario_for_leaf",
    "find_suite_path",
    "list_suite_ids",
    "load_suite",
    "with_capture_scenario",
]
