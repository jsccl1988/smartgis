# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Offline cartography prep: bbox clip, land-mask clip, OMT class normalize."""

from .bbox import clip_segment_to_bbox, pt_in_bbox
from .land_mask import clip_line_to_land_rings, point_in_rings, rings_from_polygons
from .normalize import normalize_hydro_props, normalize_road_props

__all__ = [
    "clip_line_to_land_rings",
    "clip_segment_to_bbox",
    "normalize_hydro_props",
    "normalize_road_props",
    "point_in_rings",
    "pt_in_bbox",
    "rings_from_polygons",
]
