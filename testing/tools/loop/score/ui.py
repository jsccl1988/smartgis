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
        for i, (r, g, b) in enumerate(pixels)
        if 20 <= r <= 90
        and 20 <= g <= 90
        and 20 <= b <= 90
        and abs(r - g) < 12
        and abs(g - b) < 12
        and not (
            (w // 6) <= (i % w) < ((w * 78) // 100)
            and (h // 8) <= (i // w) < ((h * 78) // 100)
        )
    )
    dark_f = dark_chrome / n
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
    left_text_f = left_text / left_n
    # TOC body (below catalog tab headers): reject empty Layers page that still
    # passes on tab-label light_text alone (visual_review empty TOC).
    toc_y0 = max(h // 8, (h * 18) // 100)
    toc_y1 = max(toc_y0 + 1, (h * 55) // 100)
    toc_pixels = [
        pixels[y * w + x]
        for y in range(toc_y0, toc_y1, 3)
        for x in range(0, left_w, 3)
    ]
    toc_n = max(1, len(toc_pixels))
    toc_text = sum(
        1
        for r, g, b in toc_pixels
        if r > 150 and g > 150 and b > 150 and abs(r - g) < 40 and abs(g - b) < 40
    )
    toc_text_f = toc_text / toc_n
    # Collapsed / empty catalog is almost pure dark; require chrome width plus
    # readable label mass (visual_review: empty TOC false-green on dark_f alone).
    catalog_strip_ok = (
        left_w >= 100
        and left_signal_f > 0.18
        and left_text_f >= 0.004
        and toc_text_f >= 0.002
    )

    # Lower ~40%: reject vertical collapse (map upper half + black dead zone).
    bottom_y0 = (h * 3) // 5
    bottom_pixels = pixels[bottom_y0 * w :]
    bottom_n = max(1, len(bottom_pixels))
    bottom_black = sum(
        1 for r, g, b in bottom_pixels if r < 12 and g < 12 and b < 12
    )
    bottom_black_f = bottom_black / bottom_n
    # Console strip is dark chrome, not pure near-black; collapse looks empty.
    no_panel_collapse = bottom_black_f < 0.55

    # Diagnostic Tools body: compact Console dock sits just above StatusBar
    # (visual_review #1). Sample ~88–96% so the band is not still map canvas.
    diag_y0 = (h * 88) // 100
    diag_y1 = (h * 96) // 100
    diag_x0 = w // 8
    diag_x1 = (w * 7) // 8
    diag_band = [
        pixels[y * w + x]
        for y in range(diag_y0, max(diag_y0 + 1, diag_y1), 2)
        for x in range(diag_x0, diag_x1, 3)
    ]
    diag_n = max(1, len(diag_band))
    diag_light = sum(
        1
        for r, g, b in diag_band
        if r > 150 and g > 150 and b > 150 and abs(r - g) < 40 and abs(g - b) < 40
    )
    diag_accent = sum(
        1
        for r, g, b in diag_band
        if (abs(r - 0) < 40 and abs(g - 122) < 55 and abs(b - 204) < 55 and b > r + 60)
        or (abs(r - 76) < 40 and abs(g - 139) < 40 and abs(b - 245) < 40)  # lane blue
        or (abs(r - 61) < 40 and abs(g - 184) < 40 and abs(b - 140) < 40)  # lane green
        or (abs(r - 230) < 40 and abs(g - 162) < 40 and abs(b - 60) < 40)  # lane amber
    )
    diag_signal_f = (diag_light + diag_accent) / diag_n
    # Empty black void under Console/Trace tabs fails; Trace gantt / empty-state
    # text / log ink passes.
    diag_content_ok = diag_signal_f >= 0.003

    # chrome_readable (checklist): hard floor on light label mass. Tuned on
    # out/Debug/captures/ui/*.bmp — near-zero ~0.0012 (catalog/data/scene) and
    # low-contrast shell ~0.0034 both stayed green when accent_or_text ORed on
    # blue alone; 0.008 rejects those while remaining reachable once product
    # contrast raises readable white/gray labels (~1% of chrome pixels).
    chrome_readable = text_f >= 0.008

    # Work-area hollow metrics (visual_review #2/#9). Reported for agents; hard
    # fail stays on no_panel_collapse + catalog_strip until map carto is stable
    # under parallel //src/gis rebuilds (center_signal often ~0.03 on hollow).
    cy0, cy1 = h // 5, (h * 3) // 5
    cx0, cx1 = w // 5, (w * 4) // 5
    center = [
        pixels[y * w + x]
        for y in range(cy0, cy1, 3)
        for x in range(cx0, cx1, 3)
    ]
    center_n = max(1, len(center))
    center_non_chrome = sum(
        1
        for r, g, b in center
        if not (
            20 <= r <= 90
            and 20 <= g <= 90
            and 20 <= b <= 90
            and abs(r - g) < 12
            and abs(g - b) < 12
        )
        and not (r < 12 and g < 12 and b < 12)
    )
    center_signal_f = center_non_chrome / center_n
    # Require a non-hollow map work area. dark_f alone false-greened shells with
    # PrintWindow map holes (center_signal≈0.0016) while chrome looked fine.
    work_area_ok = center_signal_f > 0.02

    top_menu_h = max(1, h // 25)
    top_menu_w = max(1, (w * 42) // 100)
    top_menu_n = max(1, top_menu_h * top_menu_w)
    top_menu_light = 0
    for y in range(top_menu_h):
        row = pixels[y * w : (y + 1) * w]
        for r, g, b in row[:top_menu_w]:
            if r > 180 and g > 180 and b > 180:
                top_menu_light += 1
    top_menu_light_f = top_menu_light / top_menu_n

    status_rows = pixels[(h * 96) // 100 * w :]
    status_n = max(1, len(status_rows))
    status_light = sum(
        1
        for r, g, b in status_rows
        if r > 160 and g > 160 and b > 160 and abs(r - g) < 40 and abs(g - b) < 40
    )
    status_text_f = status_light / status_n
    status_readable = status_text_f >= 0.008

    # Right inspector header: Tools/Feature plus overflow chevron (visual_review #2).
    insp_y0 = max(1, h // 12)
    insp_y1 = max(insp_y0 + 1, h // 5)
    insp_x0 = (w * 72) // 100
    insp_band = [
        pixels[y * w + x]
        for y in range(insp_y0, insp_y1, 2)
        for x in range(insp_x0, w, 3)
    ]
    insp_n = max(1, len(insp_band))
    insp_light = sum(
        1
        for r, g, b in insp_band
        if r > 160 and g > 160 and b > 160 and abs(r - g) < 40 and abs(g - b) < 40
    )
    insp_text_f = insp_light / insp_n
    inspector_tabs_ok = insp_text_f >= 0.01

    # Idle Feature dock must not eat the map (visual_review #2). Right 22%
    # should still contain map/cream in the work band, not only chrome.
    mid_y0 = h // 4
    mid_y1 = (h * 55) // 100
    right_x0 = (w * 78) // 100
    right_band = [
        pixels[y * w + x]
        for y in range(mid_y0, mid_y1, 4)
        for x in range(right_x0, w, 3)
    ]
    right_n = max(1, len(right_band))
    right_chrome = sum(
        1
        for r, g, b in right_band
        if 18 <= r <= 95
        and 18 <= g <= 95
        and 18 <= b <= 95
        and abs(r - g) < 14
        and abs(g - b) < 14
    )
    inspector_not_huge = (right_chrome / right_n) < 0.92

    # Packed icon+label chips under the menu (HiDPI 3200x2000: ~h/20).
    ambox_y0 = max(1, h // 22)
    ambox_y1 = max(ambox_y0 + 1, (h * 16) // 100)
    ambox_x0 = w // 40
    ambox_x1 = (w * 78) // 100
    ambox_band = [
        pixels[y * w + x]
        for y in range(ambox_y0, ambox_y1, 2)
        for x in range(ambox_x0, ambox_x1, 2)
    ]
    ambox_n = max(1, len(ambox_band))
    ambox_light = sum(
        1
        for r, g, b in ambox_band
        if r > 140 and g > 140 and b > 140 and abs(r - g) < 50 and abs(g - b) < 50
    )
    ambox_text_f = ambox_light / ambox_n
    map_nav_toolbar_ok = ambox_text_f >= 0.012

    # Map hero just above the compact Diagnostic dock (visual_review #2).
    # A tall stacked bottom chrome paints this band as dark panel.
    hero_y0 = (h * 62) // 100
    hero_y1 = (h * 78) // 100
    hero_x0 = w // 6
    hero_x1 = (w * 78) // 100
    hero_band = [
        pixels[y * w + x]
        for y in range(hero_y0, max(hero_y0 + 1, hero_y1), 3)
        for x in range(hero_x0, hero_x1, 3)
    ]
    hero_n = max(1, len(hero_band))
    hero_map = sum(
        1
        for r, g, b in hero_band
        if not (
            20 <= r <= 90
            and 20 <= g <= 90
            and 20 <= b <= 90
            and abs(r - g) < 12
            and abs(g - b) < 12
        )
        and not (r < 12 and g < 12 and b < 12)
    )
    hero_map_f = hero_map / hero_n
    lower_map_ok = hero_map_f >= 0.28
    chrome_density_ok = dark_f < 0.58

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
        and no_panel_collapse
        and work_area_ok
        and diag_content_ok
        and top_menu_light_f >= 0.006
        and status_readable
        and inspector_tabs_ok
        and inspector_not_huge
        and map_nav_toolbar_ok
        and lower_map_ok
        and chrome_density_ok
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
        "left_catalog_toc_text_frac": round(toc_text_f, 4),
        "bottom_near_black_frac": round(bottom_black_f, 4),
        "center_signal_frac": round(center_signal_f, 4),
        "top_menu_light_frac": round(top_menu_light_f, 4),
        "status_bar_text_frac": round(status_text_f, 4),
        "inspector_tab_text_frac": round(insp_text_f, 4),
        "inspector_right_chrome_frac": round(right_chrome / right_n, 4),
        "map_nav_toolbar_text_frac": round(ambox_text_f, 4),
        "diag_content_signal_frac": round(diag_signal_f, 4),
        "lower_map_frac": round(hero_map_f, 4),
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
            "left_catalog_toc_text>=0.002": toc_text_f >= 0.002,
            "no_panel_collapse": no_panel_collapse,
            "work_area_not_hollow": work_area_ok,
            "diag_content_signal>=0.003": diag_content_ok,
            "top_menu_light_frac>=0.006": top_menu_light_f >= 0.006,
            "status_bar_text_frac>=0.008": status_readable,
            "inspector_tab_text>=0.01": inspector_tabs_ok,
            "inspector_not_huge": inspector_not_huge,
            "map_nav_toolbar_text>=0.012": map_nav_toolbar_ok,
            "lower_map_frac>=0.28": lower_map_ok,
            "dark_chrome_frac<0.58": chrome_density_ok,
        },
    }


def score_ui_interact(path: Path) -> dict:
    """2D return chrome plus HUD/land, and sibling 3D DEM capture if present."""
    from .plugin import score_plugin_scene3d

    base = score_ui_shell_dark(path)
    w, h, pixels = load_bmp_rgb(path)
    # Map work area (skip catalog / inspector).
    x0, x1 = w // 6, (w * 78) // 100
    y0, y1 = h // 8, (h * 78) // 100
    ocean = 0
    yellow = 0
    land = 0
    orange = 0
    n_map = 0
    hud_y0 = max(0, h // 12)
    hud_y1 = min(h, h // 5)
    for y in range(y0, y1):
        row = y * w
        for x in range(x0, x1, 2):
            r, g, b = pixels[row + x]
            n_map += 1
            if abs(r - 170) < 16 and abs(g - 211) < 16 and abs(b - 223) < 16:
                ocean += 1
            # China MapLibre land is cream/beige (#f5f0e6), not carto-green.
            if (g > r + 8 and g > 90 and g > b) or (
                r > 175 and g > 165 and b > 140 and abs(r - g) < 45
            ):
                land += 1
            # Leftover GPU tessellation / OutputSurface clear (~RGB 192,128,64).
            if r > 140 and 60 < g < 180 and b < 110 and r > g + 20 and r > b + 40:
                orange += 1
            if hud_y0 <= y < hud_y1 and r > 160 and g > 160 and b < 130:
                yellow += 1
    n_map = max(1, n_map)
    ocean_f = ocean / n_map
    land_f = land / n_map
    orange_f = orange / n_map
    yellow_n = yellow
    gates = dict(base.get("gates") or {})
    gates["interact_map_w>=400"] = w >= 400
    gates["interact_map_h>=280"] = h >= 280
    gates["interact_map_ocean_frac<0.70"] = ocean_f < 0.70
    # China carto after Phase C pan/fit typically lands ~0.03-0.05 in the
    # scored map crop; 0.04 false-failed green showcase passes (0.032).
    gates["interact_map_land_frac>0.03"] = land_f > 0.03
    gates["interact_orange_tess_frac<0.20"] = orange_f < 0.20
    gates["interact_hud_yellow>=20"] = yellow_n >= 8
    sibling = path.with_name("ui-showcase-interact-3d.bmp")
    gates["interact_3d_bmp"] = sibling.is_file()
    if sibling.is_file():
        s3 = score_plugin_scene3d(sibling)
        g3 = dict(s3.get("gates") or {})
        gates["interact_3d_ocean_clear<0.90"] = bool(
            g3.get("ocean_clear_frac<0.90")
        )
        gates["interact_3d_navy_clear<0.85"] = bool(
            g3.get("navy_clear_frac<0.85")
        )
        gates["interact_3d_green_land>0.025"] = bool(
            g3.get("green_land_frac>0.025")
        )
        gates["interact_3d_neon_green<0.35"] = bool(
            g3.get("neon_green_frac<0.35")
        )
        engine_txt = sibling.with_name(sibling.stem + ".engine.txt")
        engine = ""
        if engine_txt.is_file():
            try:
                engine = engine_txt.read_text(encoding="utf-8", errors="ignore").strip()
            except OSError:
                engine = ""
        # Product SoT is FlyCube; soft GDI export is a harness fail.
        eu = engine.upper()
        engine_ok = bool(engine) and "FLYCUBE" in eu and "GDI" not in eu
        if not engine:
            # Legacy sibling without sidecar: reject soft-sky GDI clear dominance.
            ocean_clear = float(s3.get("ocean_clear_frac") or 0.0)
            engine_ok = ocean_clear < 0.55
        gates["interact_3d_engine_not_gdi"] = engine_ok
        base["interact_3d"] = {
            "bmp": str(sibling),
            "green_land_frac": s3.get("green_land_frac"),
            "ocean_clear_frac": s3.get("ocean_clear_frac"),
            "navy_clear_frac": s3.get("navy_clear_frac"),
            "engine": engine or None,
        }
    else:
        gates["interact_3d_engine_not_gdi"] = False
    base["interact_map_ocean_frac"] = round(ocean_f, 4)
    base["interact_map_land_frac"] = round(land_f, 4)
    base["interact_orange_tess_frac"] = round(orange_f, 4)
    base["interact_hud_yellow"] = yellow_n
    base["gates"] = gates
    extra = (
        "interact_map_w>=400",
        "interact_map_h>=280",
        "interact_map_ocean_frac<0.70",
        "interact_map_land_frac>0.03",
        "interact_orange_tess_frac<0.20",
        "interact_hud_yellow>=20",
        "interact_3d_bmp",
        "interact_3d_ocean_clear<0.90",
        "interact_3d_navy_clear<0.85",
        "interact_3d_green_land>0.025",
        "interact_3d_neon_green<0.35",
        "interact_3d_engine_not_gdi",
    )
    base["ok"] = bool(base.get("ok")) and all(gates.get(k) for k in extra)
    return base


