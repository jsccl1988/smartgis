# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Land-mask clip for line features (CRS84 lon/lat rings)."""

from __future__ import annotations

from typing import Sequence


def _ring_bbox(ring: Sequence[Sequence[float]]) -> tuple[float, float, float, float]:
    xs = [float(p[0]) for p in ring]
    ys = [float(p[1]) for p in ring]
    return min(xs), min(ys), max(xs), max(ys)


def _point_in_ring(x: float, y: float, ring: Sequence[Sequence[float]]) -> bool:
    # Even-odd ray cast; ring is closed or open (last==first optional).
    n = len(ring)
    if n < 3:
        return False
    inside = False
    j = n - 1
    for i in range(n):
        xi, yi = float(ring[i][0]), float(ring[i][1])
        xj, yj = float(ring[j][0]), float(ring[j][1])
        if ((yi > y) != (yj > y)) and (
            x < (xj - xi) * (y - yi) / ((yj - yi) or 1e-30) + xi
        ):
            inside = not inside
        j = i
    return inside


def point_in_rings(x: float, y: float, rings: Sequence[Sequence[Sequence[float]]]) -> bool:
    for ring in rings:
        if len(ring) < 3:
            continue
        minx, miny, maxx, maxy = _ring_bbox(ring)
        if x < minx or x > maxx or y < miny or y > maxy:
            continue
        if _point_in_ring(x, y, ring):
            return True
    return False


def rings_from_polygons(features: Sequence[dict]) -> list[list[list[float]]]:
    """Extract exterior rings from GeoJSON Polygon / MultiPolygon features."""
    rings: list[list[list[float]]] = []
    for feat in features:
        geom = feat.get("geometry") or {}
        t = geom.get("type")
        coords = geom.get("coordinates") or []
        if t == "Polygon":
            if coords:
                rings.append([[float(p[0]), float(p[1])] for p in coords[0]])
        elif t == "MultiPolygon":
            for poly in coords:
                if poly:
                    rings.append([[float(p[0]), float(p[1])] for p in poly[0]])
    return rings


def clip_line_to_land_rings(
    coords: list,
    rings: Sequence[Sequence[Sequence[float]]],
    *,
    min_on_land_frac: float = 0.25,
    max_bridge_verts: int = 8,
    max_bridge_deg: float = 0.06,
) -> list[list]:
    """Keep on-land runs for a polyline; drop ocean-only stubs.

    Returns a list of coordinate runs (each ≥2 vertices). Empty → drop feature.
    Offline stand-in for the former runtime china land-clip (roads): require
    enough samples on land; bridge only tiny gaps between adjacent on-land runs.
    """
    if not coords or len(coords) < 2 or not rings:
        return []

    pts = [[float(p[0]), float(p[1])] for p in coords]
    n = len(pts)

    runs: list[tuple[int, int]] = []
    i = 0
    on_land_total = 0
    while i < n:
        while i < n and not point_in_rings(pts[i][0], pts[i][1], rings):
            i += 1
        begin = i
        while i < n and point_in_rings(pts[i][0], pts[i][1], rings):
            i += 1
        length = i - begin
        if length >= 2:
            runs.append((begin, length))
            on_land_total += length

    if not runs:
        return []
    if on_land_total < min_on_land_frac * n:
        return []

    def run_end(run: tuple[int, int]) -> int:
        return run[0] + run[1]

    def can_bridge(a: tuple[int, int], b: tuple[int, int]) -> bool:
        if b[0] < run_end(a):
            return False
        gap = b[0] - run_end(a)
        if gap > max_bridge_verts:
            return False
        pa = pts[run_end(a) - 1]
        pb = pts[b[0]]
        dx = pb[0] - pa[0]
        dy = pb[1] - pa[1]
        return dx * dx + dy * dy <= max_bridge_deg * max_bridge_deg

    best_begin = 0
    best_end = 1
    best_score = runs[0][1]
    for start in range(len(runs)):
        score = runs[start][1]
        end = start + 1
        while end < len(runs) and can_bridge(runs[end - 1], runs[end]):
            score += runs[end][1]
            end += 1
        if score > best_score:
            best_score = score
            best_begin = start
            best_end = end

    kept: list[list[float]] = []
    for r in range(best_begin, best_end):
        begin, length = runs[r]
        kept.extend(pts[begin : begin + length])

    if len(kept) < 2:
        return []
    return [kept]
