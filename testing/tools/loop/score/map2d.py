# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Map2d BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

def score_map2d_china(path: Path) -> dict:
    """China map2d: carto vectors + DEM jet hypsometric hillshade."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))

    redish = sum(
        1
        for r, g, b in pixels
        if r > 180 and g < 160 and b < 140 and r > b + 40
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
    # Cream land (#f5f3e9) plus soft hillshade multiply (darkened cream still
    # counts as land — otherwise ok fails when DEM underlay is visible).
    land_cream = sum(
        1
        for r, g, b in pixels
        if abs(r - 245) < 55
        and abs(g - 243) < 55
        and abs(b - 233) < 55
        and r + g > b * 1.5
        and r > 170
        and g > 165
        and not (b > r + 15 and b > 140 and g > 120)
    )
    line_blue = sum(
        1
        for r, g, b in pixels
        if abs(r - 21) < 40
        and abs(g - 101) < 50
        and abs(b - 192) < 50
        and b > r + 80
    )
    road_gold = sum(
        1
        for r, g, b in pixels
        if r > 160 and g > 120 and b < 140 and r > b + 30 and abs(r - g) < 80
    )
    # Dual-stroke casing (#8a7040-ish) beside cream land.
    road_casing = sum(
        1
        for r, g, b in pixels
        if abs(r - 138) < 40
        and abs(g - 112) < 40
        and abs(b - 64) < 45
        and r > b + 20
        and g > b
    )
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    ocean_exact = sum(
        1
        for r, g, b in pixels
        if abs(r - 170) < 12 and abs(g - 211) < 12 and abs(b - 223) < 12
    )
    # Admin face #2c2418 (charcoal umber); keep off hillshade mid-grays.
    admin_gray = sum(
        1
        for r, g, b in pixels
        if abs(r - 44) < 18
        and abs(g - 36) < 18
        and abs(b - 24) < 18
        and r > b
        and (r - b) >= 8
        and r < 70
        and g < 58
    )
    soft_river = sum(
        1
        for r, g, b in pixels
        if abs(r - 100) < 35
        and abs(g - 160) < 40
        and abs(b - 208) < 40
        and b > r + 40
    )

    # Hillshade underlay: muted gray/brown relief (not cream land, not water).
    # Soft gate — only enforced when enough signal that DEM bake was painted.
    hillshade_gray = sum(
        1
        for r, g, b in pixels
        if 40 < r < 210
        and 40 < g < 210
        and 40 < b < 200
        and abs(r - g) < 28
        and abs(g - b) < 35
        and abs(r - b) < 40
        and not (
            abs(r - 245) < 28 and abs(g - 243) < 28 and abs(b - 233) < 28
        )
        and not (b > r + 15 and b > 140 and g > 120)
    )
    # Jet elevation ramp × Lambert (product default): chroma land, not cream.
    hypsometric = sum(
        1
        for r, g, b in pixels
        if (max(r, g, b) - min(r, g, b)) >= 40
        and r + g + b > 90
        and not (abs(r - 170) < 12 and abs(g - 211) < 12 and abs(b - 223) < 12)
        and not (
            abs(r - 245) < 55
            and abs(g - 243) < 55
            and abs(b - 233) < 55
            and r > 170
        )
    )
    jet_low = sum(
        1
        for r, g, b in pixels
        if b > r + 25 and b > 80 and (max(r, g, b) - min(r, g, b)) >= 40
    )
    jet_high = sum(
        1
        for r, g, b in pixels
        if r > g + 20 and r > b + 40 and r > 120 and g < 200
    )
    # Sampled luminance std as non-flat metric over a coarse grid.
    step = max(1, w // 64)
    lumas = []
    for y in range(0, h, step):
        for x in range(0, w, step):
            r, g, b = pixels[y * w + x]
            lumas.append(0.299 * r + 0.587 * g + 0.114 * b)
    mean_l = sum(lumas) / max(1, len(lumas))
    var_l = sum((v - mean_l) ** 2 for v in lumas) / max(1, len(lumas))
    luma_std = var_l**0.5

    red_f = redish / n
    salmon_f = salmon / n
    water_f = water_blue / n
    land_f = land_cream / n
    line_f = line_blue / n
    gold_f = road_gold / n
    casing_f = road_casing / n
    black_f = near_black / n
    ocean_f = ocean_exact / n
    admin_f = admin_gray / n
    river_f = soft_river / n
    hs_gray_f = hillshade_gray / n
    hypo_f = hypsometric / n
    jet_low_f = jet_low / n
    jet_high_f = jet_high / n
    detail_f = (
        line_f + gold_f + casing_f + admin_f + river_f + min(black_f, 0.03)
    )

    # Soft hillshade: only enforce non-flat when a strong gray-relief signal
    # suggests DEM bake was painted; missing china_dem must not fail china.
    # After soft-shade opacity bump, treat lower gray frac as "active".
    hs_active = hs_gray_f > 0.02 or hypo_f > 0.08
    hs_ok = (not hs_active) or (luma_std > 8.0)

    # Hillshade / admin wash can steal cream land into gray; treat admin,
    # DEM relief, and jet hypsometric land as land-like.
    land_like_f = land_f + admin_f + (hs_gray_f if hs_gray_f > 0.02 else 0.0)
    if hypo_f > 0.08:
        land_like_f += hypo_f
    jet_ok = (not (hypo_f > 0.08)) or (jet_low_f > 0.02 and jet_high_f > 0.01)
    # Bare cream north of DEM (Mongolia slab) after hillshade is active.
    cream_ok = (not hs_active) or (land_f < 0.045)
    # Shell HWND hollow after browse stress: chrome teal (#aad3df) + flat
    # admin gray fill with zero roads — water_blue/land_like soft-pass falsely.
    chrome_hollow = (ocean_f + admin_f) > 0.95 and (gold_f + casing_f) < 0.0005

    # Interior land holes: ocean clear with land on ≥3 of 4 cardinal rays.
    # Catches ear-clip / PolyPolygon cancel that soft land_like gates miss
    # (labels float over ocean inside the china landmass).
    def _is_land(r: int, g: int, b: int) -> bool:
        return (
            abs(r - 245) < 55
            and abs(g - 243) < 55
            and abs(b - 233) < 55
            and r + g > b * 1.5
            and r > 170
            and g > 165
            and not (b > r + 15 and b > 140 and g > 120)
        )

    def _is_ocean_exact(r: int, g: int, b: int) -> bool:
        return abs(r - 170) < 12 and abs(g - 211) < 12 and abs(b - 223) < 12

    hole_hits = 0
    hole_samples = 0
    hole_step = max(1, min(w, h) // 80)
    ray = max(8, min(w, h) // 40)
    for y in range(ray, h - ray, hole_step):
        for x in range(ray, w - ray, hole_step):
            hole_samples += 1
            r, g, b = pixels[y * w + x]
            if not _is_ocean_exact(r, g, b):
                continue
            landish = 0
            for dx, dy in ((0, -ray), (0, ray), (-ray, 0), (ray, 0)):
                rr, gg, bb = pixels[(y + dy) * w + (x + dx)]
                if _is_land(rr, gg, bb):
                    landish += 1
            if landish >= 3:
                hole_hits += 1
    land_hole_f = hole_hits / max(1, hole_samples)

    def _is_hs_gray(r: int, g: int, b: int) -> bool:
        return (
            40 < r < 210
            and 40 < g < 210
            and 40 < b < 200
            and abs(r - g) < 28
            and abs(g - b) < 35
            and abs(r - b) < 40
            and not (abs(r - 245) < 28 and abs(g - 243) < 28 and abs(b - 233) < 28)
            and not (b > r + 15 and b > 140 and g > 120)
        )

    cream_island_hits = 0
    cream_island_samples = 0
    for y in range(ray, h - ray, hole_step):
        for x in range(ray, w - ray, hole_step):
            cream_island_samples += 1
            r, g, b = pixels[y * w + x]
            if not _is_land(r, g, b):
                continue
            shaded = 0
            for dx, dy in ((0, -ray), (0, ray), (-ray, 0), (ray, 0)):
                rr, gg, bb = pixels[(y + dy) * w + (x + dx)]
                if _is_hs_gray(rr, gg, bb):
                    shaded += 1
            if shaded >= 3:
                cream_island_hits += 1
    cream_island_f = cream_island_hits / max(1, cream_island_samples)
    cream_island_ok = (not hs_active) or (cream_island_f < 0.004)

    size_ok = w >= 1280 and h >= 720
    admin_ok = admin_f > 0.00045

    ok = (
        red_f < 0.28
        and salmon_f < 0.08
        and water_f > 0.08
        and land_like_f > 0.18
        and detail_f > 0.004
        and black_f < 0.06
        and ocean_f < 0.82
        and land_like_f + water_f > 0.55
        and land_like_f > ocean_f * 0.20
        and (gold_f + casing_f) > 0.0012
        and casing_f > 0.00025
        and hs_ok
        and jet_ok
        and cream_ok
        and not chrome_hollow
        and land_hole_f < 0.006
        and cream_island_ok
        and size_ok
        and admin_ok
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "redish_frac": round(red_f, 4),
        "salmon_frac": round(salmon_f, 4),
        "water_blue_frac": round(water_f, 4),
        "land_cream_frac": round(land_f, 4),
        "line_blue_frac": round(line_f, 4),
        "road_gold_frac": round(gold_f, 4),
        "road_casing_frac": round(casing_f, 4),
        "near_black_frac": round(black_f, 4),
        "ocean_exact_frac": round(ocean_f, 4),
        "admin_gray_frac": round(admin_f, 4),
        "soft_river_frac": round(river_f, 4),
        "detail_frac": round(detail_f, 4),
        "hillshade_gray_frac": round(hs_gray_f, 4),
        "hypsometric_frac": round(hypo_f, 4),
        "jet_low_frac": round(jet_low_f, 4),
        "jet_high_frac": round(jet_high_f, 4),
        "hillshade_luma_std": round(luma_std, 2),
        "hillshade_active": hs_active,
        "land_interior_hole_frac": round(land_hole_f, 4),
        "cream_island_frac": round(cream_island_f, 4),
        "ok": ok,
        "gates": {
            "redish_frac<0.28": red_f < 0.28,
            "salmon_frac<0.08": salmon_f < 0.08,
            "water_blue_frac>0.08": water_f > 0.08,
            "land_like_frac>0.18": land_like_f > 0.18,
            "detail_frac>0.004": detail_f > 0.004,
            "near_black_frac<0.06": black_f < 0.06,
            "ocean_exact_frac<0.82": ocean_f < 0.82,
            "land+water>0.55": land_like_f + water_f > 0.55,
            "land_like>0.20*ocean": land_like_f > ocean_f * 0.20,
            "road_gold+casing>0.0012": (gold_f + casing_f) > 0.0012,
            "road_casing_frac>0.00025": casing_f > 0.00025,
            "hillshade_soft_ok": hs_ok,
            "jet_elev_ok": jet_ok,
            "land_cream_frac<0.045_when_hs": cream_ok,
            "not_chrome_admin_hollow": not chrome_hollow,
            "land_interior_hole_frac<0.006": land_hole_f < 0.006,
            "cream_island_frac<0.004_when_hs": cream_island_ok,
            "min_1280x720": size_ok,
            "admin_gray_frac>0.00045": admin_ok,
        },
    }



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


