# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Plugin product / map2d / scene3d / mesh BMP score gates."""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb
from .legacy import score_legacy_scene3d_china

def score_plugin_print(path: Path) -> dict:
    """Print layout: non-blank map panel plus legend/scale chrome ink."""
    base = score_plugin_product(path)
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    # Legend panel sits on the right ~160px of a 640 page — expect gray wash.
    legend_x0 = int(w * 0.72)
    legend = pixels
    legend_gray = 0
    legend_n = 0
    for y in range(h):
        row = y * w
        for x in range(legend_x0, w):
            r, g, b = pixels[row + x]
            legend_n += 1
            if abs(r - 244) < 20 and abs(g - 244) < 20 and abs(b - 244) < 20:
                legend_gray += 1
    # Scale bar: near-black horizontal stroke in the footer band (RGB sum<96
    # catches pure black and legacy 0x20 gray bars).
    footer = pixels[(h * 5 // 6) * w :]
    dark_footer = sum(1 for r, g, b in footer if r + g + b < 96)
    legend_f = legend_gray / max(1, legend_n)
    footer_dark_f = dark_footer / max(1, len(footer))
    gates = dict(base.get("gates") or {})
    gates["legend_panel_gray>0.08"] = legend_f > 0.08
    gates["scale_footer_dark>0.001"] = footer_dark_f > 0.001
    # Reject stacked city-name doubles: near-identical dark ink pairs in the
    # map panel (two glyphs ~6–22px apart vertically) inflate local ink density.
    map_x1 = int(w * 0.70)
    map_y1 = int(h * 0.85)
    dark_cells = []
    cell = 8
    for y in range(0, map_y1, cell):
        for x in range(0, map_x1, cell):
            ink = 0
            for dy in range(cell):
                for dx in range(cell):
                    xx, yy = x + dx, y + dy
                    if xx >= map_x1 or yy >= map_y1:
                        continue
                    r, g, b = pixels[yy * w + xx]
                    if r + g + b < 140 and r < 90 and g < 90 and b < 90:
                        ink += 1
            if ink >= 6:
                dark_cells.append((x, y))
    twin = 0
    for i, (x0, y0) in enumerate(dark_cells):
        for x1, y1 in dark_cells[i + 1 :]:
            if abs(x0 - x1) <= cell and 6 <= abs(y0 - y1) <= 24:
                twin += 1
                break
    twin_f = twin / max(1, len(dark_cells))
    # Print pages must not carry city glyph ink in the map panel — legend
    # lists Cities. Count dark+white halo pairs (text-like) in the map face.
    text_like = 0
    cell16 = 16
    for y in range(0, map_y1 - cell16, 8):
        for x in range(0, map_x1 - cell16, 8):
            dark_n = 0
            white_n = 0
            for dy in range(cell16):
                for dx in range(cell16):
                    xx, yy = x + dx, y + dy
                    if xx >= map_x1 or yy >= map_y1:
                        continue
                    r, g, b = pixels[yy * w + xx]
                    s = r + g + b
                    if s < 140 and r < 90:
                        dark_n += 1
                    if r > 220 and g > 220 and b > 220:
                        white_n += 1
            if dark_n >= 4 and white_n >= 4:
                text_like += 1
    map_cells = max(1, ((map_x1 // 8) * (map_y1 // 8)))
    text_like_f = text_like / map_cells
    gates["label_twin_frac<0.08"] = twin_f < 0.08
    gates["map_text_like_frac<0.02"] = text_like_f < 0.02
    base["legend_gray_frac"] = round(legend_f, 4)
    base["footer_dark_frac"] = round(footer_dark_f, 5)
    base["label_twin_frac"] = round(twin_f, 4)
    base["map_text_like_frac"] = round(text_like_f, 4)
    base["gates"] = gates
    base["ok"] = bool(base.get("ok")) and all(
        gates[k]
        for k in (
            "legend_panel_gray>0.08",
            "scale_footer_dark>0.001",
            "label_twin_frac<0.08",
            "map_text_like_frac<0.02",
        )
    )
    return base


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
    or dense heat fills on the ocean wash. Also reject a single solid blue
    rectangle (flood bbox false-green) and near-empty wash (toy traffic).
    """
    base = score_plugin_product(path)
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    divers = len(uniq)
    blue = sum(
        1
        for r, g, b in pixels
        if b > 120 and b > r + 25 and b > g + 10 and abs(g - r) < 50
    )
    blue_f = blue / n
    ink_f = float(base.get("ink_ratio") or 0.0)
    wash_f = float(base.get("wash_ratio") or 0.0)
    gates = dict(base.get("gates") or {})
    gates["color_buckets>=3"] = divers >= 3
    # Solid blue rect on gray: blue dominates ink and diversity is tiny.
    gates["not_solid_blue_rect"] = not (blue_f > 0.35 and divers < 8)
    # Toy empty maps sit near wash_ratio~0.99 with almost no structure.
    gates["wash_ratio<0.985"] = wash_f < 0.985
    gates["ink_or_structure"] = ink_f >= 0.008 or divers >= 6
    # Mid-complexity network/heat: reject near-blank cream with 2–3 buckets.
    gates["color_buckets>=4"] = divers >= 4
    gates["ink_ratio>=0.015"] = ink_f >= 0.015
    # Reject monochrome flood blob / pale traffic wash (need land+signal colors).
    greenish = sum(
        1
        for r, g, b in pixels
        if g > r + 6 and g > b + 4 and g > 70 and (r + g + b) > 140
    )
    green_f = greenish / n
    gates["land_or_heat_green>0.02"] = green_f > 0.02 or divers >= 10
    # Toy flood: blue_frac mid + tiny diversity; mid mosaic has more buckets.
    gates["not_toy_blue_blob"] = not (0.05 < blue_f < 0.55 and divers < 7)
    base["color_buckets"] = divers
    base["blue_frac"] = round(blue_f, 5)
    base["greenish_frac"] = round(green_f, 5)
    base["gates"] = gates
    base["ok"] = bool(base.get("ok")) and all(
        gates[k]
        for k in (
            "color_buckets>=3",
            "color_buckets>=4",
            "not_solid_blue_rect",
            "wash_ratio<0.985",
            "ink_or_structure",
            "ink_ratio>=0.015",
            "land_or_heat_green>0.02",
            "not_toy_blue_blob",
        )
    )
    return base



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
    # FlyCube / bad BitBlt false-PASS: near-neon green clear RGB(~20,255,0)
    # satisfies green_land_frac while product inspect is unusable.
    neon = sum(
        1 for r, g, b in pixels if g > 240 and r < 45 and b < 45
    )
    neon_f = neon / n
    # Rough diversity: unique RGB buckets on a stride sample.
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    divers = len(uniq)
    wash_f = max(ocean_f, cream_f)
    base["ocean_clear_frac"] = round(ocean_f, 4)
    base["cream_wash_frac"] = round(cream_f, 4)
    base["navy_clear_frac"] = round(navy_f, 4)
    base["neon_green_frac"] = round(neon_f, 4)
    base["color_buckets"] = divers
    # Honest green gate: do not OR-bypass with landish (false-PASS on brown
    # DEM / dark cluster when green_land_frac≈0).
    green_f = float(base.get("green_land_frac") or 0.0)
    green_ok = green_f > 0.025
    gates = dict(base.get("gates") or {})
    gates["green_land_frac>0.025"] = green_ok
    gates["ocean_clear_frac<0.90"] = ocean_f < 0.90
    gates["flat_wash_frac<0.97"] = wash_f < 0.97
    gates["color_buckets>=4"] = divers >= 4
    gates["navy_clear_frac<0.85"] = navy_f < 0.85
    gates["neon_green_frac<0.35"] = neon_f < 0.35
    base["gates"] = gates
    base["ok"] = all(bool(v) for v in gates.values())
    return base


def score_plugin_stormsurge(path: Path) -> dict:
    """Storm-surge Scene3D: DEM landish + visible cyan free-surface water.

    plugin_scene3d / legacy_scene3d_china pass without water pixels; this gate
    requires water_on_land so a dry green DEM capture fails. Also reject
    two-block abstract AABB toys (very low color diversity).
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
    # Bright cyan free-surface (albedo ~36,200,240). Exclude GDI ocean plane
    # RGB(120,190,230) which previously false-greened water_on_land (~0.42).
    def is_ocean_plane(r: int, g: int, b: int) -> bool:
        return abs(r - 120) < 28 and abs(g - 190) < 28 and abs(b - 230) < 28

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
        and not is_ocean_plane(r, g, b)
    )
    land_f = landish / n
    wol_f = water_on_land / n
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    black_f = near_black / n
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    divers = len(uniq)
    # Dark wireframe edge ink (GDI overlay) — topology signal.
    edge = sum(
        1
        for r, g, b in pixels
        if r < 80 and g < 90 and b < 100 and (r + g + b) > 40 and (r + g + b) < 220
    )
    edge_f = edge / n
    # Reject ~10x8 toy DEM (very low edge density + tiny water body).
    # Mid coast DEM + water TIN needs more edge ink than a sparse rect.
    ok = (
        land_f > 0.04
        and wol_f > 0.02
        and black_f < 0.90
        and divers >= 12
        and edge_f > 0.012
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
        "color_buckets": divers,
        "edge_frac": round(edge_f, 5),
        "ok": ok,
        "gates": {
            "landish_frac>0.04": land_f > 0.04,
            "water_on_land_frac>0.02": wol_f > 0.02,
            "near_black_frac<0.90": black_f < 0.90,
            "color_buckets>=12": divers >= 12,
            "edge_frac>0.012": edge_f > 0.012,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


def score_plugin_mesh(path: Path) -> dict:
    """Scene3D overlay TIN / gray mesh BMPs (mine stratum, prism sticks).

    Hypsometric landish greens are often absent; require non-black signal and
    reject near-empty / magenta clear only. Reject single floating yellow
    AABB toys via diversity + amber stick signal.
    """
    w, h, pixels = load_bmp_rgb(path)
    n = max(1, len(pixels))
    top_rows = pixels[: max(1, h // 3) * w]
    pink = sum(1 for r, g, b in top_rows if r > 180 and g < 140 and b < 180)
    near_black = sum(1 for r, g, b in pixels if r < 25 and g < 25 and b < 25)
    pink_f = pink / max(1, len(top_rows))
    black_f = near_black / n
    signal_f = 1.0 - black_f
    sample = pixels[:: max(1, n // 4000)]
    uniq = {(r >> 3, g >> 3, b >> 3) for r, g, b in sample}
    divers = len(uniq)
    amber = sum(
        1
        for r, g, b in pixels
        if r > 180 and g > 140 and b < 120 and r > b + 40
    )
    amber_f = amber / n
    purple = sum(
        1
        for r, g, b in pixels
        if r > 80 and b > 100 and g < 100 and b > g + 20
    )
    purple_f = purple / n
    # Orthogrid3d amber/steel hex shell — exclude light-blue ocean plane wash.
    steel = sum(
        1
        for r, g, b in pixels
        if b > 90
        and g > 70
        and r < 140
        and b > r + 10
        and (r + g + b) > 120
        and not (b > 180 and g > 150 and abs(g - b) < 55)
    )
    steel_f = steel / n
    # Dark wireframe edge ink — required for surface+wireframe 3D SoT.
    edge = sum(
        1
        for r, g, b in pixels
        if r < 80 and g < 90 and b < 100 and (r + g + b) > 40 and (r + g + b) < 220
    )
    edge_f = edge / n
    # GDI ocean pad RGB(120,190,230) — reject DEM-apron-dominated hex frames.
    ocean = sum(
        1
        for r, g, b in pixels
        if abs(r - 120) < 28 and abs(g - 190) < 28 and abs(b - 230) < 28
    )
    ocean_f = ocean / n
    # Ocean plane alone must not pass stick_or_stratum (was false-green).
    # Amber hex shell must own a visible fraction of the frame (not a tiny pad).
    structure = (
        amber_f > 0.12
        or purple_f > 0.008
        or (steel_f > 0.04 and edge_f > 0.02)
    )
    ok = (
        pink_f < 0.25
        and black_f < 0.92
        and signal_f > 0.12
        and divers >= 8
        and structure
        and amber_f < 0.45
        and edge_f > 0.002
        and ocean_f < 0.35
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
        "color_buckets": divers,
        "amber_frac": round(amber_f, 5),
        "purple_frac": round(purple_f, 5),
        "steel_frac": round(steel_f, 5),
        "edge_frac": round(edge_f, 5),
        "ocean_plane_frac": round(ocean_f, 5),
        "ok": ok,
        "gates": {
            "pink_frac_top<0.25": pink_f < 0.25,
            "near_black_frac<0.92": black_f < 0.92,
            "non_black_frac>0.12": signal_f > 0.12,
            "color_buckets>=8": divers >= 8,
            "stick_or_stratum": structure,
            "amber_frac<0.45": amber_f < 0.45,
            "edge_frac>0.002": edge_f > 0.002,
            "ocean_plane_frac<0.35": ocean_f < 0.35,
            "min_size_320x240": w >= 320 and h >= 240,
        },
    }


