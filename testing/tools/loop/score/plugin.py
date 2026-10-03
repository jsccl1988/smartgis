# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Plugin product / map2d / scene3d / mesh BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb
from .legacy import score_legacy_scene3d_china

def score_plugin_product(path: Path) -> dict:
    """Generic non-blank paint for plugin product BMPs.

    Accepts both busy meshes (world3d) and sparse ink on cream wash
    (orthogrid / orthogrid3d wireframes): dark strokes still sum >80 and
    must not be counted as wash via lit_ratio alone.

    Scene3D product captures (mine / flood 3D HWND) clear to FlyCube navy
    ≈ (0,51,102) rather than MapLibre cream — treat that as wash so a full
    navy+terrain frame is not rejected as ink_ratio==1.0.

    Map2d clear / ocean ≈ (170,211,223) (#aad3df) is also wash — a solid
    clear frame must fail (blank geochem/flood/traffic false greens).
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    lit = sum(1 for r, g, b in pixels if r + g + b > 80)
    dark = sum(1 for r, g, b in pixels if r + g + b < 40)

    def is_wash(r: int, g: int, b: int) -> bool:
        if abs(r - 245) + abs(g - 240) + abs(b - 230) < 40:
            return True
        if abs(r - 0) + abs(g - 51) + abs(b - 102) < 40:
            return True
        # Map2d ocean / default carto background (#aad3df).
        if abs(r - 170) + abs(g - 211) + abs(b - 223) < 18:
            return True
        return False

    ink = sum(1 for r, g, b in pixels if not is_wash(r, g, b))
    lit_ratio = lit / n
    dark_ratio = dark / n
    ink_ratio = ink / n
    wash_ratio = 1.0 - ink_ratio
    ok = (
        w >= 320
        and h >= 240
        and ink_ratio >= 0.003
        and ink_ratio <= 0.92
        and dark_ratio < 0.99
        and wash_ratio < 0.997
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "lit_ratio": round(lit_ratio, 5),
        "dark_ratio": round(dark_ratio, 5),
        "ink_ratio": round(ink_ratio, 5),
        "wash_ratio": round(wash_ratio, 5),
        "ok": ok,
        "gates": {
            "min_size_320x240": w >= 320 and h >= 240,
            "ink_ratio>=0.003": ink_ratio >= 0.003,
            "ink_ratio<=0.92": ink_ratio <= 0.92,
            "dark_ratio<0.99": dark_ratio < 0.99,
            "wash_ratio<0.997": wash_ratio < 0.997,
        },
    }


def score_plugin_map2d(path: Path) -> dict:
    """Map2d plugin showcases (geochem / flood / traffic).

    Reject solid ocean clear and cream-only empty frames; accept sparse ink
    or dense heat fills on the ocean wash.
    """
    return score_plugin_product(path)



def score_plugin_scene3d(path: Path) -> dict:
    """Views product BMPs (world3d / map2d plugin traffic·flood·geochem).

    Full-frame DEM / TIN fills nearly every pixel off the MapLibre cream wash,
    so plugin_product's ink_ratio<=0.85 rejects legitimate captures. Reuse the
    legacy Scene3D landish / non-black gates, but reject flat Map2d ocean clear
    (#aad3df) which the landish predicate misclassifies as 100% land.
    """
    base = score_legacy_scene3d_china(path)
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    # Map2d ocean / plugin style bg RGB(170,211,223).
    ocean = sum(
        1
        for r, g, b in pixels
        if abs(r - 170) < 14 and abs(g - 211) < 14 and abs(b - 223) < 14
    )
    cream = sum(
        1
        for r, g, b in pixels
        if abs(r - 245) < 14 and abs(g - 240) < 14 and abs(b - 230) < 14
    )
    ocean_f = ocean / n
    cream_f = cream / n
    # FlyCube init clear RGB(18,32,48) — solid navy = no DEM present yet.
    navy = sum(
        1
        for r, g, b in pixels
        if abs(r - 18) < 10 and abs(g - 32) < 12 and abs(b - 48) < 14
    )
    navy_f = navy / n
    # Rough diversity: unique RGB buckets on a stride sample.
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    divers = len(uniq)
    wash_f = max(ocean_f, cream_f)
    base["ocean_clear_frac"] = round(ocean_f, 4)
    base["cream_wash_frac"] = round(cream_f, 4)
    base["navy_clear_frac"] = round(navy_f, 4)
    base["color_buckets"] = divers
    gates = dict(base.get("gates") or {})
    gates["ocean_clear_frac<0.90"] = ocean_f < 0.90
    gates["flat_wash_frac<0.97"] = wash_f < 0.97
    gates["color_buckets>=4"] = divers >= 4
    gates["navy_clear_frac<0.85"] = navy_f < 0.85
    base["gates"] = gates
    base["ok"] = (
        bool(base.get("ok"))
        and ocean_f < 0.90
        and wash_f < 0.97
        and divers >= 4
        and navy_f < 0.85
    )
    return base


def score_plugin_stormsurge(path: Path) -> dict:
    """Storm-surge Scene3D: DEM landish + visible cyan free-surface water.

    plugin_scene3d / legacy_scene3d_china pass without water pixels; this gate
    requires water_on_land so a dry green DEM capture fails.
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    landish = sum(
        1
        for r, g, b in pixels
        if (g > r + 8 and g > b + 5 and g > 70)
        or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
        or (r > 140 and g > 140 and abs(r - g) < 40 and r + g > b * 1.5)
        # Dark teal DEM bed (FlyCube overlay) — not bright free-surface cyan.
        or (
            g > 45
            and b > 45
            and g > r + 15
            and b > r + 15
            and abs(g - b) < 45
            and (r + g + b) < 280
            and (r + g + b) > 90
        )
    )
    # Bright cyan/teal free-surface (albedo ~46,170,220), not dark void navy.
    water_on_land = sum(
        1
        for r, g, b in pixels
        if b >= 140
        and g >= 120
        and r < 130
        and b > r + 40
        and g > r + 30
        and abs(g - b) < 100
        and (r + g + b) > 280
        and not (g > b + 25)
    )
    land_f = landish / n
    wol_f = water_on_land / n
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    black_f = near_black / n
    ok = (
        land_f > 0.04
        and wol_f > 0.008
        and black_f < 0.92
        and w >= 320
        and h >= 240
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "landish_frac": round(land_f, 4),
        "water_on_land_frac": round(wol_f, 4),
        "near_black_frac": round(black_f, 4),
        "ok": ok,
        "gates": {
            "landish_frac>0.04": land_f > 0.04,
            "water_on_land_frac>0.008": wol_f > 0.008,
            "near_black_frac<0.92": black_f < 0.92,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


def score_plugin_mesh(path: Path) -> dict:
    """Scene3D overlay TIN / gray mesh BMPs (mine stratum, prism sticks).

    Hypsometric landish greens are often absent; require non-black signal and
    reject near-empty / magenta clear only.
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    top_rows = pixels[: max(1, h // 3) * w]
    pink = sum(1 for r, g, b in top_rows if r > 180 and g < 140 and b < 180)
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    pink_f = pink / max(1, len(top_rows))
    black_f = near_black / n
    signal_f = 1.0 - black_f
    ok = (
        pink_f < 0.25
        and black_f < 0.92
        and signal_f > 0.12
        and w >= 320
        and h >= 240
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "pink_frac_top": round(pink_f, 4),
        "near_black_frac": round(black_f, 4),
        "non_black_frac": round(signal_f, 4),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.25": pink_f < 0.25,
            "near_black_frac<0.92": black_f < 0.92,
            "non_black_frac>0.12": signal_f > 0.12,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


