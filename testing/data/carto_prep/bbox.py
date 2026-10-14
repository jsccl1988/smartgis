# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""CRS84 bbox helpers for offline line extracts."""

from __future__ import annotations


def pt_in_bbox(
    x: float, y: float, bbox: tuple[float, float, float, float]
) -> bool:
    return bbox[0] <= x <= bbox[2] and bbox[1] <= y <= bbox[3]


def clip_segment_to_bbox(
    coords: list, bbox: tuple[float, float, float, float]
) -> list[list]:
    """Keep contiguous runs of vertices inside bbox; drop foreign stubs."""
    runs: list[list] = []
    cur: list = []
    for pt in coords:
        x, y = float(pt[0]), float(pt[1])
        if pt_in_bbox(x, y, bbox):
            cur.append([x, y])
        else:
            if len(cur) >= 2:
                runs.append(cur)
            cur = []
    if len(cur) >= 2:
        runs.append(cur)
    return runs
