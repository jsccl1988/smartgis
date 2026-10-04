# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Named BMP visual gates (score_id) for suite contracts.

Implementations live in family modules (ui / map2d / atmosphere / plugin /
legacy / views). This module is the stable dispatch facade.
"""

from __future__ import annotations

from pathlib import Path

from .atmosphere import score_atmosphere_full, score_atmosphere_globe
from .legacy import (
    score_legacy_map2d_china,
    score_legacy_scene3d_china,
    score_legacy_scene3d_hud,
    score_legacy_scene3d_mesh,
    score_legacy_scene3d_points,
)
from .map2d import score_map2d_china, score_map2d_orthogrid
from .plugin import (
    score_plugin_map2d,
    score_plugin_mesh,
    score_plugin_print,
    score_plugin_product,
    score_plugin_scene3d,
    score_plugin_stormsurge,
)
from .ui import score_ui_shell_dark
from .views import score_views_present_dxgi, score_views_shell_chrome

_SCORE_FNS = {
    "ui_shell_dark": score_ui_shell_dark,
    "map2d_china": score_map2d_china,
    "map2d_orthogrid": score_map2d_orthogrid,
    "plugin_product": score_plugin_product,
    "plugin_print": score_plugin_print,
    "plugin_map2d": score_plugin_map2d,
    "plugin_scene3d": score_plugin_scene3d,
    "plugin_stormsurge": score_plugin_stormsurge,
    "plugin_mesh": score_plugin_mesh,
    "atmosphere_full": score_atmosphere_full,
    "atmosphere_globe": score_atmosphere_globe,
    "legacy_map2d_china": score_legacy_map2d_china,
    "legacy_scene3d_china": score_legacy_scene3d_china,
    "legacy_scene3d_mesh": score_legacy_scene3d_mesh,
    "legacy_scene3d_hud": score_legacy_scene3d_hud,
    "legacy_scene3d_points": score_legacy_scene3d_points,
    "views_shell_chrome": score_views_shell_chrome,
    "views_present_dxgi": score_views_present_dxgi,
}


def score_bmp(path: Path, score_id: str) -> dict:
    fn = _SCORE_FNS.get(score_id)
    if fn is None:
        known = ", ".join(sorted(_SCORE_FNS))
        raise KeyError(f"unknown bmp score_id={score_id!r} (known: {known})")
    return fn(path)


def known_score_ids() -> list[str]:
    return sorted(_SCORE_FNS)
