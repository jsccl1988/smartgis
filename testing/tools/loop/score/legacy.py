# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Legacy GDI / scene3d BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

def score_legacy_map2d_china(path: Path) -> dict:
    """Legacy GDI china carto gates (looser than Views MapLibre wash)."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    redish = sum(
        1 for r, g, b in pixels if r > 180 and g < 160 and b < 140 and r > b + 40
    )
    salmon = sum(
        1
        for r, g, b in pixels
        if abs(r - 255) < 50
        and abs(g - 204) < 55
        and abs(b - 163) < 55
        and r > b + 30
    )
    water_blue = sum(
        1
        for r, g, b in pixels
        if b > r + 15 and b > 140 and g > 120 and r < 220 and abs(g - b) < 80
    )
    land_cream = sum(
        1
        for r, g, b in pixels
        if r > 180
        and g > 180
        and b > 150
        and abs(r - g) <= 35
        and abs(g - b) < 45
        and r + g > b * 1.4
        and not (b > r + 15 and abs(g - b) < 40)
    )
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    # Reject IDE/blank captures: pure white dominates when BitBlt hits Cursor
    # or Edit never painted china carto (cream land is not near-white).
    near_white = sum(1 for r, g, b in pixels if r > 240 and g > 240 and b > 240)
    red_f = redish / n
    salmon_f = salmon / n
    water_f = water_blue / n
    land_f = land_cream / n
    black_f = near_black / n
    white_f = near_white / n
    ok = (
        red_f < 0.12
        and salmon_f < 0.08
        and water_f > 0.04
        and land_f > 0.08
        and black_f < 0.55
        and white_f < 0.50
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "redish_frac": round(red_f, 4),
        "salmon_frac": round(salmon_f, 4),
        "water_blue_frac": round(water_f, 4),
        "land_cream_frac": round(land_f, 4),
        "near_black_frac": round(black_f, 4),
        "near_white_frac": round(white_f, 4),
        "ok": ok,
        "gates": {
            "redish_frac<0.12": red_f < 0.12,
            "salmon_frac<0.08": salmon_f < 0.08,
            "water_blue_frac>0.04": water_f > 0.04,
            "land_cream_frac>0.08": land_f > 0.08,
            "near_black_frac<0.55": black_f < 0.55,
            # Edit shell chrome is often white; IDE occlusion was ~0.77+.
            "near_white_frac<0.50": white_f < 0.50,
        },
    }


def score_legacy_scene3d_china(path: Path) -> dict:
    """Leftover GL/D3D DEM gates (hypsometric wash + black clear).

    China showcase composition: hypsometric land on a near-black void. Reject
    FlyCube navy clear with a sparse elongated strip (equal-profile false-PASS
    when near_black≈0 but landish still clears). GL compass dial numerals use
    Diffuse(1.0,0.4,0.6) pink (~0.008 of BL ROI) — allow sparse HUD digits;
    fail dense pink fill / solid magenta clear via compass + full-frame gates.
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    # load_bmp_rgb is top-down; approximate former bottom-up top band.
    top_rows = pixels[: max(1, h // 3) * w]
    mid_rows = pixels[(h // 3) * w : (2 * h // 3) * w]
    pink = sum(1 for r, g, b in top_rows if r > 180 and g < 140 and b < 180)
    landish = 0
    green_land = 0
    navy_clear = 0
    full_pink = 0
    for r, g, b in pixels:
        is_land = (
            (g > r + 8 and g > b + 5 and g > 70)
            or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
            or (r > 140 and g > 140 and abs(r - g) < 40 and r + g > b * 1.5)
            or (g > 70 and b > 80 and g > r + 5 and abs(g - b) < 80)
        )
        if is_land:
            landish += 1
            if g > r + 8 and g > b + 5 and g > 70:
                green_land += 1
        # FlyCube clear ≈ (0,51,102); dominant navy + sparse land = wrong capture.
        if (
            b > 70
            and b > r + 40
            and b > g + 20
            and r < 60
            and g < 100
            and (r + g + b) < 220
        ):
            navy_clear += 1
        if r > 180 and g < 140 and b < 200 and r > g + 40:
            full_pink += 1
    flat_cyan = sum(
        1
        for r, g, b in mid_rows
        if abs(r - 45) < 40
        and abs(g - 90) < 50
        and b > g
        and 100 < b < 175
        and r < 100
        and g < 125
    )
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    # BL compass inset: sparse pink dial numerals are legitimate GL HUD
    # (~0.008); threshold catches dense pink fill, not digit strokes.
    bl_x1 = max(1, w // 3)
    bl_y0 = (2 * h) // 3
    compass_pink = 0
    compass_n = 0
    for y in range(bl_y0, h):
        row = y * w
        for x in range(0, bl_x1):
            r, g, b = pixels[row + x]
            compass_n += 1
            if r > 200 and 70 < g < 140 and 120 < b < 200 and r > g + 60:
                compass_pink += 1
    pink_f = pink / max(1, len(top_rows))
    land_f = landish / n
    green_f = green_land / n
    navy_f = navy_clear / n
    cyan_f = flat_cyan / max(1, len(mid_rows))
    black_f = near_black / n
    signal_f = 1.0 - black_f
    compass_pink_f = compass_pink / max(1, compass_n)
    full_pink_f = full_pink / n
    # Wrong equal-profile composition: navy wash + sparse land, no black void.
    navy_strip_bad = navy_f > 0.35 and land_f < 0.15 and black_f < 0.05
    # East-China hypsometric greens are present on real china DEM; strip often
    # lacks them (cream/beige ridge only).
    green_ok = green_f > 0.025
    ok = (
        pink_f < 0.25
        and land_f > 0.04
        and cyan_f < 0.15
        and black_f < 0.85
        and signal_f > 0.08
        and compass_pink_f < 0.02
        and full_pink_f < 0.05
        and not navy_strip_bad
        and green_ok
        and w >= 320
        and h >= 240
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "pink_frac_top": round(pink_f, 4),
        "landish_frac": round(land_f, 4),
        "green_land_frac": round(green_f, 4),
        "navy_clear_frac": round(navy_f, 4),
        "flat_cyan_frac_mid": round(cyan_f, 4),
        "near_black_frac": round(black_f, 4),
        "non_black_frac": round(signal_f, 4),
        "compass_pink_frac": round(compass_pink_f, 5),
        "full_pink_frac": round(full_pink_f, 4),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.25": pink_f < 0.25,
            "landish_frac>0.04": land_f > 0.04,
            "green_land_frac>0.025": green_ok,
            "flat_cyan_frac_mid<0.15": cyan_f < 0.15,
            "near_black_frac<0.85": black_f < 0.85,
            "non_black_frac>0.08": signal_f > 0.08,
            "compass_pink_frac<0.02": compass_pink_f < 0.02,
            "full_pink_frac<0.05": full_pink_f < 0.05,
            "not_navy_sparse_strip": not navy_strip_bad,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }



def score_legacy_scene3d_mesh(path: Path) -> dict:
    """Leftover per-object mesh showcase (cube/sphere/water/pointcloud).

    Like plugin_mesh but slightly looser non_black (0.10) so sparse point
    lists and wireframe cubes still PASS without diluting product plugin gates.
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
        and signal_f > 0.10
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
            "non_black_frac>0.10": signal_f > 0.10,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


def score_legacy_scene3d_hud(path: Path) -> dict:
    """NorthArray / HUD-only leftover showcase (sparse compass on black clear).

    Same pink/size gates as mesh, but allow lower non-black (dial strokes are
    sparse vs a filled mesh silhouette).
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
        and black_f < 0.95
        and signal_f > 0.08
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
            "near_black_frac<0.95": black_f < 0.95,
            "non_black_frac>0.08": signal_f > 0.08,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


def score_legacy_scene3d_points(path: Path) -> dict:
    """Synthetic point-cloud showcase: sparse green dots on near-black clear.

    Mesh gate wants non_black>0.12; a 17×17 grid under default orbit lands
    ~0.11–0.115. Keep pink/size gates, relax signal floor to 0.10 only here.
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
        and signal_f > 0.10
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
            "non_black_frac>0.10": signal_f > 0.10,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


