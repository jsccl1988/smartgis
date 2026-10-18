# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Map2d BMP score gates.

China looks are design contracts in map2d_contract.py. This module only
dispatches a score_id. Orthogrid stays a separate mesh gate.
"""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb
from .map2d_contract import score_contract


def score_map2d_china_dem(path: Path) -> dict:
    """China overview: jet hypsometric hillshade plus vectors."""
    return score_contract(path, "map2d_china_dem")


def score_map2d_china_carto(path: Path) -> dict:
    """China overview: cream land and vectors, no DEM wash."""
    return score_contract(path, "map2d_china_carto")


def score_map2d_align(path: Path) -> dict:
    """style_align.json still. Cream, water, river, roads. No admin layer."""
    return score_contract(path, "map2d_align")


def score_map2d_orthogrid(path: Path) -> dict:
    """Orthogrid mesh + heat-ramp gates (not wireframe-only on cream)."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    bright = sum(1 for r, g, b in pixels if r + g + b > 600)
    dark = sum(1 for r, g, b in pixels if r + g + b < 420 and max(r, g, b) < 180)

    def is_cream(r: int, g: int, b: int) -> bool:
        return abs(r - 245) + abs(g - 240) + abs(b - 230) < 40

    def is_heatish(r: int, g: int, b: int) -> bool:
        # Skip cream wash and near-black mesh ink.
        if is_cream(r, g, b):
            return False
        if r + g + b < 120 and max(r, g, b) < 80:
            return False
        # Blended green/yellow/red fills over cream keep measurable chroma.
        return (max(r, g, b) - min(r, g, b)) >= 25

    heat = sum(1 for r, g, b in pixels if is_heatish(r, g, b))
    edges = 0
    samples = 0
    step = max(1, w // 80)
    for y in range(0, h - 1, step):
        for x in range(0, w - 1, step):
            i = y * w + x
            j = y * w + (x + 1)
            lum0 = sum(pixels[i])
            lum1 = sum(pixels[j])
            if abs(lum0 - lum1) > 40:
                edges += 1
            samples += 1
    edge_ratio = edges / max(1, samples)
    dark_ratio = dark / n
    bright_ratio = bright / n
    heat_ratio = heat / n
    ok = (
        w >= 320
        and h >= 240
        and dark_ratio >= 0.002
        and edge_ratio >= 0.02
        and bright_ratio < 0.995
        and heat_ratio >= 0.01
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "dark_ratio": round(dark_ratio, 5),
        "bright_ratio": round(bright_ratio, 5),
        "edge_ratio": round(edge_ratio, 5),
        "heat_ratio": round(heat_ratio, 5),
        "ok": ok,
        "gates": {
            "min_size_320x240": w >= 320 and h >= 240,
            "dark_ratio>=0.002": dark_ratio >= 0.002,
            "edge_ratio>=0.02": edge_ratio >= 0.02,
            "bright_ratio<0.995": bright_ratio < 0.995,
            "heat_ratio>=0.01": heat_ratio >= 0.01,
        },
    }
