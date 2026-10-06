# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Harness / captures path constants and scenario prefixing."""

from __future__ import annotations

from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[2]
HARNESS_DIR = TOOLS_DIR / "harness"
SHARED_DIR = HARNESS_DIR / "_shared"
ROOT = TOOLS_DIR.parents[1]

# Top-level dirs under captures/ that are not suite scenario folders.
_CAPTURE_RESERVED_TOP = frozenset({"record", "analysis", "_scratch"})


def capture_scenario_for_leaf(leaf: str) -> str | None:
    """Mirror C++ capture_scenario_prefix_* for flat basenames."""
    name = leaf.replace("\\", "/").strip("/")
    if not name or "/" in name:
        return None
    if name.startswith(("plugin-", "plugin_")):
        return "plugin"
    if name.startswith(
        (
            "atmosphere-",
            "atmosphere_",
            "map2d-",
            "map2d_",
            "browser-",
            "browser_",
        )
    ):
        return "browser"
    if name.startswith(("ui-", "ui_")):
        return "ui"
    if name.startswith(("legacy-", "legacy_")):
        return "legacy"
    if name.startswith(
        (
            "input-",
            "input_",
            "harness-",
            "harness_",
            "self-test-",
            "views-plain-",
            "views_plain_",
            "browse_",
            "browse-",
            "console_",
            "console-",
        )
    ):
        return "shell"
    if name.startswith("_"):
        return "_scratch"
    return None


def with_capture_scenario(leaf: str, family: str | None = None) -> str:
    """Prefix |leaf| with scenario dir when still a flat basename."""
    norm = leaf.replace("\\", "/").lstrip("/")
    if not norm or "/" in norm:
        return norm
    top = norm.split("/", 1)[0]
    if top in _CAPTURE_RESERVED_TOP:
        return norm
    scenario = capture_scenario_for_leaf(norm)
    if scenario is None and family:
        scenario = family
    if scenario:
        return f"{scenario}/{norm}"
    return norm


def resolve_capture_leaf(
    root: Path, leaf: str, family: str | None
) -> Path:
    """Primary scenario path, with flat-leaf fallback if that file exists."""
    rel = with_capture_scenario(leaf, family)
    primary = root / Path(rel)
    if primary.is_file():
        return primary
    flat = leaf.replace("\\", "/").lstrip("/")
    if "/" not in flat:
        alt = root / flat
        if alt.is_file():
            return alt
    return primary
