# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Stable import path: round scorers live in loop.score.round."""

from .score.round import (  # noqa: F401
    _score_click_gate,
    _score_fps_gate,
    _score_motion_gate,
    _score_round,
    _score_zoom_gate,
    attach_runtime_gates,
)
