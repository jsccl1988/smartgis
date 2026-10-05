# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Atmosphere showcase BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

def score_atmosphere_full(path: Path) -> dict:
    """Atmosphere full-mode sky / land visual gates (top-down pixels)."""
    w, h, pixels = load_bmp_rgb(path)
    # Top-down: first rows are image top.
    top_rows = pixels[: max(1, h // 3) * w]
    mid_rows = pixels[(h // 3) * w : (2 * h // 3) * w]
    all_rows = pixels
    zenith_rows = pixels[: max(1, h // 8) * w]
    horizon_band = pixels[max(1, h // 8) * w : max(1, h // 3) * w]

    # Magenta mid-band often sits just below the top third — score upper half.
    pink_rows = pixels[: max(1, h // 2) * w]
    pink = sum(1 for r, g, b in pink_rows if r > 150 and g < 130 and b < 190 and r > g + 25)
    # True Rayleigh zenith is deep blue (B>>G). Cyan wash (G≈B) must not
    # count as sky — that was masking a missing SkyPass draw.
    blue_sky = sum(
        1
        for r, g, b in top_rows
        if b > r + 30
        and b > g + 25
        and b > 120
        and g < 200
        and r < 160
    )
    cyan_wash = sum(
        1
        for r, g, b in top_rows
        if g > 200 and b > 200 and abs(g - b) < 40 and r < 140
    )
    landish = sum(
        1
        for r, g, b in all_rows
        if (
            (
                (g > r + 8 and g > b + 5 and g > 70)
                or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
            )
            # Reject washed sky / fog false positives (near-white pale).
            and not (r > 200 and g > 200 and b > 180)
        )
    )
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

    def mean_rgb(rows: list[tuple[int, int, int]]) -> tuple[float, float, float]:
        if not rows:
            return (0.0, 0.0, 0.0)
        n_r = len(rows)
        return (
            sum(p[0] for p in rows) / n_r,
            sum(p[1] for p in rows) / n_r,
            sum(p[2] for p in rows) / n_r,
        )

    z_m = mean_rgb(zenith_rows)
    h_m = mean_rgb(horizon_band)
    sky_delta = abs(z_m[2] - h_m[2]) + abs(z_m[0] - h_m[0])
    flat_sky = sky_delta < 12.0

    pink_f = pink / max(1, len(pink_rows))
    sky_f = blue_sky / max(1, len(top_rows))
    land_f = landish / max(1, len(all_rows))
    cyan_f = flat_cyan / max(1, len(mid_rows))
    cyan_wash_f = cyan_wash / max(1, len(top_rows))

    ok = (
        pink_f < 0.12
        and sky_f > 0.20
        and land_f > 0.03
        and cyan_f < 0.20
        and cyan_wash_f < 0.15
        and not flat_sky
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "pink_frac_top": round(pink_f, 4),
        "blue_sky_frac_top": round(sky_f, 4),
        "landish_frac_mid": round(land_f, 4),
        "flat_cyan_frac_mid": round(cyan_f, 4),
        "cyan_wash_frac_top": round(cyan_wash_f, 4),
        "sky_delta_zenith_horizon": round(sky_delta, 2),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.12": pink_f < 0.12,
            "blue_sky_frac_top>0.20": sky_f > 0.20,
            "landish_frac_mid>0.03": land_f > 0.03,
            "flat_cyan_frac_mid<0.20": cyan_f < 0.20,
            "cyan_wash_frac_top<0.15": cyan_wash_f < 0.15,
            "sky_not_flat_clear": not flat_sky,
        },
    }


def score_atmosphere_globe(path: Path) -> dict:
    """Globe-mode gates: spherical DEM limb, China land, space/starfield, not flat."""
    w, h, pixels = load_bmp_rgb(path)
    top_rows = pixels[: max(1, h // 3) * w]
    all_rows = pixels
    # Corner samples — space / sky around a centered globe limb.
    corner = []
    for y in (0, 1, 2, h - 3, h - 2, h - 1):
        for x in (0, 1, 2, w - 3, w - 2, w - 1):
            if 0 <= y < h and 0 <= x < w:
                corner.append(pixels[y * w + x])
    center = []
    for y in range(h // 3, 2 * h // 3):
        for x in range(w // 3, 2 * w // 3):
            center.append(pixels[y * w + x])

    pink = sum(
        1
        for r, g, b in top_rows
        if r > 150 and g < 130 and b < 190 and r > g + 25
    )
    # Legacy Rayleigh blue (still accepted) or deep-space corners.
    blue_sky = sum(
        1
        for r, g, b in top_rows
        if b > r + 20 and b > g + 10 and b > 90 and r < 170
    )
    dark_space = sum(1 for r, g, b in corner if (r + g + b) < 110)
    # Sparse bright star pixels in the top band (procedural starfield).
    starish = sum(
        1
        for r, g, b in top_rows
        if (r + g + b) > 420 and max(r, g, b) > 180 and abs(r - g) < 40
    )
    landish = sum(
        1
        for r, g, b in all_rows
        if (
            (
                (g > r + 8 and g > b + 5 and g > 70)
                or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
            )
            and not (r > 200 and g > 200 and b > 180)
        )
    )
    # Soft cloud shell / sat cover: pale neutrals distinct from DEM greens.
    cloudish = sum(
        1
        for r, g, b in center
        if r > 140 and g > 140 and b > 140 and abs(r - g) < 35 and abs(g - b) < 40
    )

    def mean_rgb(rows: list[tuple[int, int, int]]) -> tuple[float, float, float]:
        if not rows:
            return (0.0, 0.0, 0.0)
        n = len(rows)
        return (
            sum(p[0] for p in rows) / n,
            sum(p[1] for p in rows) / n,
            sum(p[2] for p in rows) / n,
        )

    c_m = mean_rgb(center)
    k_m = mean_rgb(corner)
    limb_contrast = abs(
        (c_m[0] + c_m[1] + c_m[2]) - (k_m[0] + k_m[1] + k_m[2])
    )
    flat_clear = (
        abs(c_m[0] - k_m[0]) < 8
        and abs(c_m[1] - k_m[1]) < 8
        and abs(c_m[2] - k_m[2]) < 8
    )

    pink_f = pink / max(1, len(top_rows))
    sky_f = blue_sky / max(1, len(top_rows))
    space_f = dark_space / max(1, len(corner))
    star_f = starish / max(1, len(top_rows))
    land_f = landish / max(1, len(all_rows))
    cloud_f = cloudish / max(1, len(center))
    space_or_sky = space_f > 0.12 or (sky_f > 0.10 and space_f > 0.08)
    size_ok = w >= 320 and h >= 240

    # China DEM is a regional patch on the unit globe — land fraction is small.
    # Plugin world3d captures the full shell HWND (often 2120×1006), not 640².
    ok = (
        pink_f < 0.12
        and space_or_sky
        and land_f > 0.008
        and limb_contrast > 40.0
        and not flat_clear
        and size_ok
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "pink_frac_top": round(pink_f, 4),
        "blue_sky_frac_top": round(sky_f, 4),
        "space_corner_frac": round(space_f, 4),
        "starish_frac_top": round(star_f, 6),
        "landish_frac": round(land_f, 4),
        "cloudish_frac_center": round(cloud_f, 4),
        "limb_contrast": round(limb_contrast, 2),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.12": pink_f < 0.12,
            "space_or_sky": space_or_sky,
            "landish_frac>0.008": land_f > 0.008,
            "limb_contrast>40": limb_contrast > 40.0,
            "not_flat_clear": not flat_clear,
            "min_size_320x240": size_ok,
        },
    }


