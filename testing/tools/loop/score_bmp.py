# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Named BMP visual gates (score_id) for suite contracts."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb


def score_ui_shell_dark(path: Path) -> dict:
    """Shell chrome gates for default dark Views theme."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    top_rows = pixels[: max(1, (h // 8) * w)]

    near_black = sum(1 for r, g, b in pixels if r < 12 and g < 12 and b < 12)
    magenta = sum(
        1
        for r, g, b in pixels
        if r > 180 and b > 180 and g < 120 and abs(r - b) < 60
    )
    dark_chrome = sum(
        1
        for r, g, b in pixels
        if 20 <= r <= 90
        and 20 <= g <= 90
        and 20 <= b <= 90
        and abs(r - g) < 12
        and abs(g - b) < 12
    )
    accent_blue = sum(
        1
        for r, g, b in pixels
        if abs(r - 0) < 40
        and abs(g - 122) < 50
        and abs(b - 204) < 50
        and b > r + 80
        and b > g
    )
    light_text = sum(
        1
        for r, g, b in pixels
        if r > 170 and g > 170 and b > 170 and abs(r - g) < 30 and abs(g - b) < 30
    )
    top_chrome = sum(
        1
        for r, g, b in top_rows
        if (20 <= r <= 100 and abs(r - g) < 15 and abs(g - b) < 15)
        or (r > 170 and g > 170 and b > 170)
        or (b > r + 60 and b > 140)
    )

    tab_bleed = 0
    accent_ys: list[int] = []
    # Packed tabs: active accent may be only ~50–120px wide on a 1920px strip.
    min_accent_samples = max(8, w // 200)
    for y in range(min(h, max(1, h // 6))):
        row = pixels[y * w : (y + 1) * w]
        accent_n = sum(
            1
            for r, g, b in row[::4]
            if abs(r - 0) < 25
            and abs(g - 122) < 25
            and abs(b - 204) < 25
            and b > r + 100
        )
        if accent_n < min_accent_samples:
            continue
        accent_ys.append(y)

    header_ys: list[int] = []
    if accent_ys:
        header_ys = [accent_ys[0]]
        for y in accent_ys[1:]:
            if y - header_ys[-1] > 2:
                break
            header_ys.append(y)

    bleed_ys = set(header_ys[:-4]) if len(header_ys) > 4 else set(header_ys)

    # Packed tabs: active accent cell must not span a third of the strip.
    max_accent_run = 0
    accent_x0 = 0
    accent_x1 = 0
    if header_ys:
        mid_y = header_ys[len(header_ys) // 2]
        row = pixels[mid_y * w : (mid_y + 1) * w]
        run = 0
        run_start = 0
        for x, (r, g, b) in enumerate(row):
            is_acc = (
                abs(r - 0) < 25
                and abs(g - 122) < 25
                and abs(b - 204) < 25
                and b > r + 100
            )
            if is_acc:
                if run == 0:
                    run_start = x
                run += 1
                if run > max_accent_run:
                    max_accent_run = run
                    accent_x0 = run_start
                    accent_x1 = x + 1
            else:
                run = 0
    accent_span_frac = max_accent_run / max(1, w)

    # Only score map-bleed inside the active accent cell.
    if accent_x1 <= accent_x0:
        accent_x0, accent_x1 = 0, 0
    for y in bleed_ys:
        row = pixels[y * w : (y + 1) * w]
        for x in range(accent_x0, accent_x1, 2):
            r, g, b = row[x]
            if abs(r - 0) < 25 and abs(g - 122) < 25 and abs(b - 204) < 25:
                continue
            if r < 100 and g < 100 and b < 100:
                continue
            if abs(r - 170) < 30 and abs(g - 211) < 30 and abs(b - 223) < 30:
                tab_bleed += 1

    buckets: set[int] = set()
    for r, g, b in pixels[:: max(1, n // 8000)]:
        buckets.add(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))

    black_f = near_black / n
    magenta_f = magenta / n
    dark_f = dark_chrome / n
    accent_f = accent_blue / n
    text_f = light_text / n
    top_f = top_chrome / max(1, len(top_rows))
    bucket_n = len(buckets)
    bleed_ok = tab_bleed < 40
    # Prefer contiguous header rows; fall back to accent fraction when the
    # active tab is content-packed (few samples per row).
    accent_painted = (len(header_ys) >= 6 and accent_f > 0.0002) or (
        accent_f > 0.0008 and max_accent_run >= 24
    ) or (accent_f > 0.001)
    packed_tabs_ok = accent_span_frac < 0.22

    ok = (
        black_f < 0.25
        and magenta_f < 0.08
        and dark_f > 0.25
        and (accent_f > 0.0003 or text_f > 0.0005)
        and top_f > 0.50
        and bucket_n >= 6
        and w >= 400
        and h >= 300
        and bleed_ok
        and accent_painted
        and packed_tabs_ok
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "near_black_frac": round(black_f, 4),
        "magenta_frac": round(magenta_f, 4),
        "dark_chrome_frac": round(dark_f, 4),
        "accent_blue_frac": round(accent_f, 4),
        "light_text_frac": round(text_f, 4),
        "top_chrome_frac": round(top_f, 4),
        "color_buckets": bucket_n,
        "tab_band_map_bleed": tab_bleed,
        "tab_accent_rows": len(header_ys),
        "tab_accent_span_frac": round(accent_span_frac, 4),
        "ok": ok,
        "gates": {
            "near_black_frac<0.25": black_f < 0.25,
            "magenta_frac<0.08": magenta_f < 0.08,
            "dark_chrome_frac>0.25": dark_f > 0.25,
            "accent_or_text": accent_f > 0.0003 or text_f > 0.0005,
            "top_chrome_frac>0.50": top_f > 0.50,
            "color_buckets>=6": bucket_n >= 6,
            "min_size_400x300": w >= 400 and h >= 300,
            "tab_band_no_map_bleed": bleed_ok,
            "tab_accent_painted": accent_painted,
            "tab_accent_span_frac<0.22": packed_tabs_ok,
        },
    }


def score_map2d_china(path: Path) -> dict:
    """MapLibre / Baidu-like carto gates for china map2d showcase."""
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
    land_cream = sum(
        1
        for r, g, b in pixels
        if abs(r - 245) < 28
        and abs(g - 243) < 28
        and abs(b - 233) < 28
        and r + g > b * 1.5
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
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    ocean_exact = sum(
        1
        for r, g, b in pixels
        if abs(r - 170) < 12 and abs(g - 211) < 12 and abs(b - 223) < 12
    )
    admin_gray = sum(
        1
        for r, g, b in pixels
        if abs(r - 196) < 25
        and abs(g - 190) < 25
        and abs(b - 176) < 25
        and r > b
    )
    soft_river = sum(
        1
        for r, g, b in pixels
        if abs(r - 100) < 35
        and abs(g - 160) < 40
        and abs(b - 208) < 40
        and b > r + 40
    )

    red_f = redish / n
    salmon_f = salmon / n
    water_f = water_blue / n
    land_f = land_cream / n
    line_f = line_blue / n
    gold_f = road_gold / n
    black_f = near_black / n
    ocean_f = ocean_exact / n
    admin_f = admin_gray / n
    river_f = soft_river / n
    detail_f = line_f + gold_f + admin_f + river_f + min(black_f, 0.03)

    ok = (
        red_f < 0.08
        and salmon_f < 0.05
        and water_f > 0.10
        and land_f > 0.18
        and detail_f > 0.004
        and black_f < 0.06
        and ocean_f < 0.75
        and land_f + water_f > 0.55
        and land_f > ocean_f * 0.35
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
        "near_black_frac": round(black_f, 4),
        "ocean_exact_frac": round(ocean_f, 4),
        "admin_gray_frac": round(admin_f, 4),
        "soft_river_frac": round(river_f, 4),
        "detail_frac": round(detail_f, 4),
        "ok": ok,
        "gates": {
            "redish_frac<0.08": red_f < 0.08,
            "salmon_frac<0.05": salmon_f < 0.05,
            "water_blue_frac>0.10": water_f > 0.10,
            "land_cream_frac>0.18": land_f > 0.18,
            "detail_frac>0.004": detail_f > 0.004,
            "near_black_frac<0.06": black_f < 0.06,
            "ocean_exact_frac<0.75": ocean_f < 0.75,
            "land+water>0.55": land_f + water_f > 0.55,
            "land>0.35*ocean": land_f > ocean_f * 0.35,
        },
    }


def score_atmosphere_full(path: Path) -> dict:
    """Atmosphere full-mode sky / land visual gates (top-down pixels)."""
    w, h, pixels = load_bmp_rgb(path)
    # Top-down: first rows are image top.
    top_rows = pixels[: max(1, h // 3) * w]
    mid_rows = pixels[(h // 3) * w : (2 * h // 3) * w]
    all_rows = pixels
    zenith_rows = pixels[: max(1, h // 8) * w]
    horizon_band = pixels[max(1, h // 8) * w : max(1, h // 3) * w]

    pink = sum(1 for r, g, b in top_rows if r > 180 and g < 140 and b < 180)
    # True Rayleigh zenith is deep blue (B>>G). Cyan wash (G≈B?55) must not
    # count as sky ?that was masking a missing SkyPass draw.
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
        if (g > r + 8 and g > b + 5 and g > 70)
        or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
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

    pink_f = pink / max(1, len(top_rows))
    sky_f = blue_sky / max(1, len(top_rows))
    land_f = landish / max(1, len(all_rows))
    cyan_f = flat_cyan / max(1, len(mid_rows))
    cyan_wash_f = cyan_wash / max(1, len(top_rows))

    ok = (
        pink_f < 0.25
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
            "pink_frac_top<0.25": pink_f < 0.25,
            "blue_sky_frac_top>0.20": sky_f > 0.20,
            "landish_frac_mid>0.03": land_f > 0.03,
            "flat_cyan_frac_mid<0.20": cyan_f < 0.20,
            "cyan_wash_frac_top<0.15": cyan_wash_f < 0.15,
            "sky_not_flat_clear": not flat_sky,
        },
    }


def score_map2d_orthogrid(path: Path) -> dict:
    """Orthogrid mesh gates: not flat wash, enough dark ink for grid lines."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    bright = sum(1 for r, g, b in pixels if r + g + b > 600)
    dark = sum(1 for r, g, b in pixels if r + g + b < 420 and max(r, g, b) < 180)
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
    ok = (
        w >= 320
        and h >= 240
        and dark_ratio >= 0.002
        and edge_ratio >= 0.02
        and bright_ratio < 0.995
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "dark_ratio": round(dark_ratio, 5),
        "bright_ratio": round(bright_ratio, 5),
        "edge_ratio": round(edge_ratio, 5),
        "ok": ok,
        "gates": {
            "min_size_320x240": w >= 320 and h >= 240,
            "dark_ratio>=0.002": dark_ratio >= 0.002,
            "edge_ratio>=0.02": edge_ratio >= 0.02,
            "bright_ratio<0.995": bright_ratio < 0.995,
        },
    }


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
    red_f = redish / n
    salmon_f = salmon / n
    water_f = water_blue / n
    land_f = land_cream / n
    black_f = near_black / n
    ok = (
        red_f < 0.12
        and salmon_f < 0.08
        and water_f > 0.04
        and land_f > 0.08
        and black_f < 0.55
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
        "ok": ok,
        "gates": {
            "redish_frac<0.12": red_f < 0.12,
            "salmon_frac<0.08": salmon_f < 0.08,
            "water_blue_frac>0.04": water_f > 0.04,
            "land_cream_frac>0.08": land_f > 0.08,
            "near_black_frac<0.55": black_f < 0.55,
        },
    }


def score_legacy_scene3d_china(path: Path) -> dict:
    """Leftover GL/D3D DEM gates (hypsometric wash + black clear)."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    # load_bmp_rgb is top-down; approximate former bottom-up top band.
    top_rows = pixels[: max(1, h // 3) * w]
    mid_rows = pixels[(h // 3) * w : (2 * h // 3) * w]
    pink = sum(1 for r, g, b in top_rows if r > 180 and g < 140 and b < 180)
    landish = sum(
        1
        for r, g, b in pixels
        if (g > r + 8 and g > b + 5 and g > 70)
        or (r > 90 and g > 80 and b < 130 and r + g > b * 2)
        or (r > 140 and g > 140 and abs(r - g) < 40 and r + g > b * 1.5)
        or (g > 70 and b > 80 and g > r + 5 and abs(g - b) < 80)
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
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    pink_f = pink / max(1, len(top_rows))
    land_f = landish / n
    cyan_f = flat_cyan / max(1, len(mid_rows))
    black_f = near_black / n
    signal_f = 1.0 - black_f
    ok = (
        pink_f < 0.25
        and land_f > 0.04
        and cyan_f < 0.15
        and black_f < 0.90
        and signal_f > 0.08
        and w >= 320
        and h >= 240
    )
    return {
        "bmp": str(path),
        "width": w,
        "height": h,
        "pink_frac_top": round(pink_f, 4),
        "landish_frac": round(land_f, 4),
        "flat_cyan_frac_mid": round(cyan_f, 4),
        "near_black_frac": round(black_f, 4),
        "non_black_frac": round(signal_f, 4),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.25": pink_f < 0.25,
            "landish_frac>0.04": land_f > 0.04,
            "flat_cyan_frac_mid<0.15": cyan_f < 0.15,
            "near_black_frac<0.90": black_f < 0.90,
            "non_black_frac>0.08": signal_f > 0.08,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


_SCORE_FNS = {
    "ui_shell_dark": score_ui_shell_dark,
    "map2d_china": score_map2d_china,
    "map2d_orthogrid": score_map2d_orthogrid,
    "atmosphere_full": score_atmosphere_full,
    "legacy_map2d_china": score_legacy_map2d_china,
    "legacy_scene3d_china": score_legacy_scene3d_china,
}


def score_bmp(path: Path, score_id: str) -> dict:
    fn = _SCORE_FNS.get(score_id)
    if fn is None:
        known = ", ".join(sorted(_SCORE_FNS))
        raise KeyError(f"unknown bmp score_id={score_id!r} (known: {known})")
    return fn(path)


def known_score_ids() -> list[str]:
    return sorted(_SCORE_FNS)
