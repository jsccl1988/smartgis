# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""UI shell BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

def score_ui_shell_dark(path: Path) -> dict:
    """Shell chrome gates for default dark Views theme."""
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    top_rows = pixels[: max(1, (h // 8) * w)]
    # Left catalog strip (first ~12%): reject a collapsed kMinPanePx pane.
    left_w = max(1, min(w // 8, 360))
    left_pixels = [
        pixels[y * w + x]
        for y in range(h // 10, max(h // 10 + 1, (h * 4) // 5), 4)
        for x in range(0, left_w, 4)
    ]

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
    # Map/Catalog/inspector strips use HeaderPlacement::kBottom — scan both
    # the top and bottom sixths of the frame.
    min_accent_samples = max(8, w // 200)
    scan_rows = list(range(min(h, max(1, h // 6))))
    bottom_start = max(0, h - max(1, h // 6))
    scan_rows.extend(range(bottom_start, h))
    for y in scan_rows:
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
        # Prefer a contiguous band (top or bottom header). Sort uniquely.
        accent_ys = sorted(set(accent_ys))
        header_ys = [accent_ys[0]]
        for y in accent_ys[1:]:
            if y - header_ys[-1] > 2:
                # Jump to a second band (e.g. top menu vs bottom tabs).
                if len(header_ys) < 4 and y >= bottom_start:
                    header_ys = [y]
                    continue
                break
            header_ys.append(y)
        # If the first contiguous run was tiny, try the densest bottom cluster.
        if len(header_ys) < 4:
            bottom_ys = [y for y in accent_ys if y >= bottom_start]
            if len(bottom_ys) >= len(header_ys):
                header_ys = [bottom_ys[0]]
                for y in bottom_ys[1:]:
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

    left_n = max(1, len(left_pixels))
    left_dark = sum(
        1
        for r, g, b in left_pixels
        if 18 <= r <= 95 and 18 <= g <= 95 and 18 <= b <= 95
        and abs(r - g) < 14
        and abs(g - b) < 14
    )
    left_text = sum(
        1
        for r, g, b in left_pixels
        if r > 160 and g > 160 and b > 160 and abs(r - g) < 35 and abs(g - b) < 35
    )
    left_accent = sum(
        1
        for r, g, b in left_pixels
        if abs(r - 0) < 40 and abs(g - 122) < 50 and abs(b - 204) < 50 and b > r + 80
    )
    left_signal_f = (left_dark + left_text + left_accent) / left_n
    # Collapsed 40px catalog is almost pure map wash / empty; require chrome
    # signal in the left strip (layout_fail regression gate).
    catalog_strip_ok = left_w >= 80 and left_signal_f > 0.12

    # chrome_readable (checklist): hard floor on light label mass. Tuned on
    # out/Debug/captures/ui/*.bmp — near-zero ~0.0012 (catalog/data/scene) and
    # low-contrast shell ~0.0034 both stayed green when accent_or_text ORed on
    # blue alone; 0.008 rejects those while remaining reachable once product
    # contrast raises readable white/gray labels (~1% of chrome pixels).
    chrome_readable = text_f >= 0.008

    ok = (
        black_f < 0.25
        and magenta_f < 0.08
        and dark_f > 0.25
        and (accent_f > 0.0003 or text_f > 0.0005)
        and chrome_readable
        and top_f > 0.50
        # Shell-only captures can be chrome-heavy (few buckets) when the map
        # hole is flat teal/dark; require at least a handful of tones.
        and bucket_n >= 5
        and w >= 400
        and h >= 300
        and bleed_ok
        and accent_painted
        and packed_tabs_ok
        and catalog_strip_ok
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
        "left_catalog_signal_frac": round(left_signal_f, 4),
        "ok": ok,
        "gates": {
            "near_black_frac<0.25": black_f < 0.25,
            "magenta_frac<0.08": magenta_f < 0.08,
            "dark_chrome_frac>0.25": dark_f > 0.25,
            "accent_or_text": accent_f > 0.0003 or text_f > 0.0005,
            "chrome_readable": chrome_readable,
            "light_text_frac>=0.008": chrome_readable,
            "top_chrome_frac>0.50": top_f > 0.50,
            "color_buckets>=5": bucket_n >= 5,
            "min_size_400x300": w >= 400 and h >= 300,
            "tab_band_no_map_bleed": bleed_ok,
            "tab_accent_painted": accent_painted,
            "tab_accent_span_frac<0.22": packed_tabs_ok,
            "left_catalog_strip_ok": catalog_strip_ok,
        },
    }


