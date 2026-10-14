# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""OpenMapTiles / MapLibre class normalize for roads and hydro lines."""

from __future__ import annotations

# OMT road class ranks used by Scenic carto (motorway > trunk > primary).
ROAD_OMT_CLASSES = frozenset({"motorway", "trunk", "primary"})

# Legacy product tags → OMT.
_ROAD_ALIAS = {
    "expressway": "motorway",
    "highway": "trunk",
    "road": "primary",
    "secondary": "primary",
    "tertiary": "primary",
    "residential": "primary",
    "unclassified": "primary",
}

# Major China rivers keep OMT class "river"; others become "stream".
_MAJOR_RIVER_NAMES = frozenset({"长江", "黄河", "珠江"})

_HYDRO_OMT = frozenset({"river", "canal", "stream", "lake"})


def normalize_road_props(props: dict) -> dict:
    """Return a shallow-copied props with kind=road and OMT class."""
    out = dict(props)
    out["kind"] = "road"
    raw = str(out.get("class") or "").strip().lower()
    if raw in ROAD_OMT_CLASSES:
        out["class"] = raw
        return out
    if raw in _ROAD_ALIAS:
        out["class"] = _ROAD_ALIAS[raw]
        return out
    out["class"] = "primary"
    return out


def normalize_hydro_props(props: dict) -> dict:
    """Return props with kind in {river,lake,canal,stream} and OMT class."""
    out = dict(props)
    kind = str(out.get("kind") or "river").strip().lower()
    if kind not in ("river", "lake", "canal", "stream", "water"):
        kind = "river"
    if kind == "water":
        kind = "river"
    out["kind"] = kind

    raw = str(out.get("class") or "").strip().lower()
    if raw in _HYDRO_OMT:
        out["class"] = raw
        return out

    name = str(out.get("name") or "").strip()
    if kind == "lake":
        out["class"] = "lake"
        return out
    if kind == "canal":
        out["class"] = "canal"
        return out
    if kind == "stream":
        out["class"] = "stream"
        return out
    # kind == river: majors stay "river", minor stems → "stream".
    if name in _MAJOR_RIVER_NAMES:
        out["class"] = "river"
    else:
        out["class"] = "stream"
    return out
