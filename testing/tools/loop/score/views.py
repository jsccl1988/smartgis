# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Views shell / Present DXGI BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

def score_views_shell_chrome(path: Path) -> dict:
    """Plain-launch shell PrintWindow: chrome OK; map hole teal is expected.

    WS_EX_NOREDIRECTIONBITMAP leaves the embed as GDI clear RGB(170,211,223).
    Do **not** treat high ocean_clear as product hollow — carto gold lives on
    FlyCube Present BitBlt (score_views_present_dxgi / map2d_china).
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    top = pixels[: max(1, (h // 6) * w)]
    ocean = sum(
        1
        for r, g, b in pixels
        if abs(r - 170) < 18 and abs(g - 211) < 18 and abs(b - 223) < 18
    )
    dark_chrome = sum(
        1
        for r, g, b in pixels
        if 18 <= r <= 95
        and 18 <= g <= 95
        and 18 <= b <= 95
        and abs(r - g) < 14
        and abs(g - b) < 14
    )
    accent = sum(
        1
        for r, g, b in top
        if r < 55 and 70 < g < 170 and 150 < b < 255 and b > g + 25
    )
    near_black = sum(1 for r, g, b in pixels if r < 12 and g < 12 and b < 12)
    light_text = sum(
        1
        for r, g, b in top
        if r > 170 and g > 170 and b > 170 and abs(r - g) < 30 and abs(g - b) < 30
    )
    ocean_f = ocean / n
    chrome_f = dark_chrome / n
    accent_f = accent / max(1, len(top))
    black_f = near_black / n
    text_f = light_text / max(1, len(top))
    # Reject near-solid navy FlyCube / present-only captures that miss menu
    # and tab chrome (ui.scene blank BMP regression).
    # Require real chrome mass — teal hole + flat navy fill alone must fail
    # (visual_review #7 chrome_readable false-green risk).
    chrome_readable = (chrome_f > 0.14 and accent_f >= 0.0015) or (
        chrome_f > 0.20 and text_f >= 0.003
    )
    ok = (
        black_f < 0.85
        and chrome_readable
        and w >= 640
    )
    return {
        "score_id": "views_shell_chrome",
        "ok": ok,
        "w": w,
        "h": h,
        "ocean_clear_frac": round(ocean_f, 4),
        "dark_chrome_frac": round(chrome_f, 4),
        "top_accent_frac": round(accent_f, 4),
        "top_light_text_frac": round(text_f, 4),
        "near_black_frac": round(black_f, 4),
        "gates": {
            "near_black_frac<0.85": black_f < 0.85,
            "chrome_readable": chrome_readable,
            "min_width_640": w >= 640,
            "teal_map_hole_allowed": True,
        },
        "note": "Shell PW teal embed is harness-only; do not score as map2d_china.",
    }


def score_views_present_dxgi(path: Path) -> dict:
    """FlyCube Present BitBlt gold: reject shell chrome bleed + flat teal hole.

    Accepts Map2D china wash / Scene3D DEM landish. Fails when BitBlt sampled
    IDE/shell TabStrip (top accent bleed) or GDI ocean clear under a covered
    present HWND.
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    band_h = max(1, h // 5)
    top = pixels[: band_h * w]
    ocean = sum(
        1
        for r, g, b in pixels
        if abs(r - 170) < 18 and abs(g - 211) < 18 and abs(b - 223) < 18
    )
    accent = sum(
        1
        for r, g, b in top
        if abs(r - 0) < 12 and abs(g - 122) < 28 and abs(b - 204) < 28
    )
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    # Do NOT count near-white (245,240,230) as land — hollow Present BitBlt
    # was false-green via that clause (visual_review #6).
    landish = sum(
        1
        for r, g, b in pixels
        if (g > r + 8 and g > b + 5 and g > 55)
        or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
        or (
            r > 140
            and g > 140
            and b < 200
            and abs(r - g) < 40
            and r + g > b * 1.5
            and not (r > 230 and g > 230 and b > 210)
        )
    )
    near_white = sum(
        1 for r, g, b in pixels if r > 230 and g > 230 and b > 210
    )
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    ocean_f = ocean / n
    bleed_f = accent / max(1, len(top))
    black_f = near_black / n
    land_f = landish / n
    white_f = near_white / n
    divers = len(uniq)
    # China carto: light land wash + teal ocean coastline + sparse city ink.
    # Cleared swapchain is near-uniform white with tiny ocean/diversity.
    coastline_ok = (
        ocean_f > 0.08
        and white_f > 0.20
        and white_f < 0.85
        and black_f < 0.08
        and ocean_f < 0.55
    )
    landish_or_diverse = land_f > 0.015 or divers >= 6 or coastline_ok
    white_ok = white_f < 0.55 or (
        white_f < 0.80 and land_f > 0.05 and divers >= 8
    ) or coastline_ok
    ok = (
        w >= 320
        and h >= 240
        and bleed_f < 0.02
        and ocean_f < 0.55
        and black_f < 0.90
        and white_ok
        # Scene3D DEM BitBlt is often low-bucket (navy/land wash) under DXGI
        # opaque capture; accept modest diversity when chrome/ocean are clean.
        and landish_or_diverse
    )
    return {
        "score_id": "views_present_dxgi",
        "ok": ok,
        "w": w,
        "h": h,
        "chrome_bleed_frac": round(bleed_f, 4),
        "ocean_clear_frac": round(ocean_f, 4),
        "near_black_frac": round(black_f, 4),
        "near_white_frac": round(white_f, 4),
        "landish_frac": round(land_f, 4),
        "color_buckets": divers,
        "gates": {
            "chrome_bleed_frac<0.02": bleed_f < 0.02,
            "ocean_clear_frac<0.55": ocean_f < 0.55,
            "near_black_frac<0.90": black_f < 0.90,
            "near_white_ok": white_ok,
            "landish_or_diverse": landish_or_diverse,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }

