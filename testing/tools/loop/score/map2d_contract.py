# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""China map2d design contracts.

Bands are measurements. A contract is a list of min/max clauses combined with
AND only. A failing still is a render or seed bug. Do not add OR substitutes,
conditional skips, or a map2d_china alias that accepts either look.
"""

from __future__ import annotations

from pathlib import Path

from .bmp_io import load_bmp_rgb

Clause = tuple[str, str, float]


def _clauses(*rows: Clause) -> tuple[Clause, ...]:
    return rows


# Shared rejection of a hollow shell and of a flat wrong-size still.
_FRAME: tuple[Clause, ...] = _clauses(
    ("redish_frac", "max", 0.28),
    ("salmon_frac", "max", 0.08),
    ("near_black_frac", "max", 0.06),
    ("ocean_exact_frac", "max", 0.82),
    ("chrome_hollow", "max", 0.0),
    ("width", "min", 1280),
    ("height", "min", 720),
)

# Product china showcase: jet hypsometric x Lambert, vectors still required.
CHINA_DEM: tuple[Clause, ...] = (
    _clauses(
        ("water_blue_frac", "min", 0.08),
        ("hypsometric_frac", "min", 0.08),
        ("jet_low_frac", "min", 0.02),
        ("jet_high_frac", "min", 0.01),
        ("hillshade_gray_frac", "min", 0.02),
        ("hillshade_luma_std", "min", 8.0),
        ("dem_land_water_frac", "min", 0.55),
        ("dem_land_vs_ocean", "min", 0.0),
        ("road_stroke_frac", "min", 0.0012),
        ("road_casing_frac", "min", 0.00025),
        ("admin_gray_frac", "min", 0.00045),
        ("soft_river_frac", "min", 0.0002),
        ("land_cream_frac", "max", 0.045),
        ("dem_hole_frac", "max", 0.006),
    )
    + _FRAME
)

# Cream vectors. Jet and hillshade gray cannot stand in for land.
CHINA_CARTO: tuple[Clause, ...] = (
    _clauses(
        ("land_cream_frac", "min", 0.18),
        ("water_blue_frac", "min", 0.08),
        ("carto_land_water_frac", "min", 0.55),
        ("carto_land_vs_ocean", "min", 0.0),
        ("road_stroke_frac", "min", 0.0012),
        ("road_casing_frac", "min", 0.00025),
        ("admin_gray_frac", "min", 0.00045),
        ("soft_river_frac", "min", 0.0002),
        ("hypsometric_frac", "max", 0.08),
        ("hillshade_gray_frac", "max", 0.02),
        ("carto_hole_frac", "max", 0.006),
        ("cream_island_frac", "max", 0.004),
    )
    + _FRAME
)

# style_align.json: cream, #aad3df water, river, road casing, road.
# That style has no admin layer, so this is not CHINA_CARTO.
ALIGN: tuple[Clause, ...] = (
    _clauses(
        ("land_cream_frac", "min", 0.18),
        ("water_blue_frac", "min", 0.08),
        ("carto_land_water_frac", "min", 0.55),
        ("carto_land_vs_ocean", "min", 0.0),
        ("road_stroke_frac", "min", 0.0012),
        ("road_casing_frac", "min", 0.00025),
        ("soft_river_frac", "min", 0.0002),
        ("hypsometric_frac", "max", 0.08),
        ("hillshade_gray_frac", "max", 0.02),
        ("carto_hole_frac", "max", 0.006),
        ("cream_island_frac", "max", 0.004),
    )
    + _FRAME
)

CONTRACTS: dict[str, tuple[Clause, ...]] = {
    "map2d_china_dem": CHINA_DEM,
    "map2d_china_carto": CHINA_CARTO,
    "map2d_align": ALIGN,
}


def _is_cream(r: int, g: int, b: int) -> bool:
    return (
        abs(r - 245) < 55
        and abs(g - 243) < 55
        and abs(b - 233) < 55
        and r + g > b * 1.5
        and r > 170
        and g > 165
        and not (b > r + 15 and b > 140 and g > 120)
    )


def _is_ocean(r: int, g: int, b: int) -> bool:
    return abs(r - 170) < 12 and abs(g - 211) < 12 and abs(b - 223) < 12


def _is_water(r: int, g: int, b: int) -> bool:
    return b > r + 15 and b > 140 and g > 120 and r < 220 and abs(g - b) < 80


def _is_hs_gray(r: int, g: int, b: int) -> bool:
    return (
        40 < r < 210
        and 40 < g < 210
        and 40 < b < 200
        and abs(r - g) < 28
        and abs(g - b) < 35
        and abs(r - b) < 40
        and not (abs(r - 245) < 28 and abs(g - 243) < 28 and abs(b - 233) < 28)
        and not _is_water(r, g, b)
    )


def _is_hypo(r: int, g: int, b: int) -> bool:
    return (
        (max(r, g, b) - min(r, g, b)) >= 40
        and r + g + b > 90
        and not _is_ocean(r, g, b)
        and not (
            abs(r - 245) < 55
            and abs(g - 243) < 55
            and abs(b - 233) < 55
            and r > 170
        )
    )


def measure_bands(w: int, h: int, pixels: list[tuple[int, int, int]]) -> dict:
    """Count named bands. Does not decide pass or fail."""
    n = max(1, len(pixels))
    redish = salmon = water = cream = line_blue = gold = casing = 0
    black = ocean = admin = river = hs_gray = hypo = jet_low = jet_high = 0
    for r, g, b in pixels:
        if r > 180 and g < 160 and b < 140 and r > b + 40:
            redish += 1
        if (
            abs(r - 255) < 50
            and abs(g - 204) < 55
            and abs(b - 163) < 55
            and r > b + 30
        ):
            salmon += 1
        if _is_water(r, g, b):
            water += 1
        if _is_cream(r, g, b):
            cream += 1
        if abs(r - 21) < 40 and abs(g - 101) < 50 and abs(b - 192) < 50 and b > r + 80:
            line_blue += 1
        if r > 160 and g > 120 and b < 140 and r > b + 30 and abs(r - g) < 80:
            gold += 1
        if (
            abs(r - 138) < 40
            and abs(g - 112) < 40
            and abs(b - 64) < 45
            and r > b + 20
            and g > b
        ):
            casing += 1
        if r < 25 and g < 25 and b < 25:
            black += 1
        if _is_ocean(r, g, b):
            ocean += 1
        if (
            abs(r - 44) < 18
            and abs(g - 36) < 18
            and abs(b - 24) < 18
            and r > b
            and (r - b) >= 8
            and r < 70
            and g < 58
        ):
            admin += 1
        if (
            abs(r - 100) < 35
            and abs(g - 160) < 40
            and abs(b - 208) < 40
            and b > r + 40
        ):
            river += 1
        if _is_hs_gray(r, g, b):
            hs_gray += 1
        if _is_hypo(r, g, b):
            hypo += 1
        if b > r + 25 and b > 80 and (max(r, g, b) - min(r, g, b)) >= 40:
            jet_low += 1
        if r > g + 20 and r > b + 40 and r > 120 and g < 200:
            jet_high += 1

    step = max(1, w // 64)
    lumas = []
    for y in range(0, h, step):
        for x in range(0, w, step):
            r, g, b = pixels[y * w + x]
            lumas.append(0.299 * r + 0.587 * g + 0.114 * b)
    mean_l = sum(lumas) / max(1, len(lumas))
    var_l = sum((v - mean_l) ** 2 for v in lumas) / max(1, len(lumas))
    luma_std = var_l**0.5

    ray = max(8, min(w, h) // 40)
    hole_step = max(1, min(w, h) // 80)

    def _hole_frac(is_land) -> float:
        hits = 0
        samples = 0
        for y in range(ray, h - ray, hole_step):
            for x in range(ray, w - ray, hole_step):
                samples += 1
                r, g, b = pixels[y * w + x]
                if not _is_ocean(r, g, b):
                    continue
                landish = 0
                for dx, dy in ((0, -ray), (0, ray), (-ray, 0), (ray, 0)):
                    rr, gg, bb = pixels[(y + dy) * w + (x + dx)]
                    if is_land(rr, gg, bb):
                        landish += 1
                if landish >= 3:
                    hits += 1
        return hits / max(1, samples)

    def _dem_land(r: int, g: int, b: int) -> bool:
        return _is_hypo(r, g, b) or _is_hs_gray(r, g, b)

    cream_island_hits = 0
    cream_island_samples = 0
    for y in range(ray, h - ray, hole_step):
        for x in range(ray, w - ray, hole_step):
            cream_island_samples += 1
            r, g, b = pixels[y * w + x]
            if not _is_cream(r, g, b):
                continue
            shaded = 0
            for dx, dy in ((0, -ray), (0, ray), (-ray, 0), (ray, 0)):
                rr, gg, bb = pixels[(y + dy) * w + (x + dx)]
                if _is_hs_gray(rr, gg, bb):
                    shaded += 1
            if shaded >= 3:
                cream_island_hits += 1

    water_f = water / n
    cream_f = cream / n
    ocean_f = ocean / n
    admin_f = admin / n
    gold_f = gold / n
    casing_f = casing / n
    hypo_f = hypo / n
    chrome_hollow = 1.0 if (ocean_f + admin_f) > 0.95 and (gold_f + casing_f) < 0.0005 else 0.0
    return {
        "width": w,
        "height": h,
        "redish_frac": redish / n,
        "salmon_frac": salmon / n,
        "water_blue_frac": water_f,
        "land_cream_frac": cream_f,
        "line_blue_frac": line_blue / n,
        "road_gold_frac": gold_f,
        "road_casing_frac": casing_f,
        "road_stroke_frac": gold_f + casing_f,
        "near_black_frac": black / n,
        "ocean_exact_frac": ocean_f,
        "admin_gray_frac": admin_f,
        "soft_river_frac": river / n,
        "hillshade_gray_frac": hs_gray / n,
        "hypsometric_frac": hypo_f,
        "jet_low_frac": jet_low / n,
        "jet_high_frac": jet_high / n,
        "hillshade_luma_std": luma_std,
        "dem_land_water_frac": hypo_f + water_f,
        "dem_land_vs_ocean": hypo_f - 0.20 * ocean_f,
        "carto_land_water_frac": cream_f + water_f,
        "carto_land_vs_ocean": cream_f - 0.20 * ocean_f,
        "dem_hole_frac": _hole_frac(_dem_land),
        "carto_hole_frac": _hole_frac(_is_cream),
        "cream_island_frac": cream_island_hits / max(1, cream_island_samples),
        "chrome_hollow": chrome_hollow,
    }


def _clause_id(metric: str, op: str, threshold: float) -> str:
    rel = ">=" if op == "min" else "<="
    return f"{metric}{rel}{threshold:g}"


def _holds(value: float, op: str, threshold: float) -> bool:
    if op == "min":
        return value + 1e-12 >= threshold
    if op == "max":
        return value <= threshold + 1e-12
    raise ValueError(f"contract op must be min or max, got {op!r}")


def evaluate_contract(metrics: dict, contract_id: str) -> dict:
    clauses = CONTRACTS.get(contract_id)
    if clauses is None:
        known = ", ".join(sorted(CONTRACTS))
        raise KeyError(f"unknown map2d contract {contract_id!r} (known: {known})")
    gates: dict[str, bool] = {}
    for metric, op, threshold in clauses:
        if metric not in metrics:
            raise KeyError(f"contract {contract_id} metric {metric} was not measured")
        gates[_clause_id(metric, op, threshold)] = _holds(float(metrics[metric]), op, threshold)
    ok = all(gates.values())
    out = {
        "contract": contract_id,
        "ok": ok,
        "gates": gates,
    }
    for key, value in metrics.items():
        if isinstance(value, float):
            out[key] = round(value, 4) if key != "hillshade_luma_std" else round(value, 2)
        else:
            out[key] = value
    return out


def score_contract(path: Path, contract_id: str) -> dict:
    w, h, pixels = load_bmp_rgb(path)
    scored = evaluate_contract(measure_bands(w, h, pixels), contract_id)
    scored["bmp"] = str(path)
    return scored
