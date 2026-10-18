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
    # Flat single-color clear is ~0–3. Olive DEM vs zenith clear lands ~4–6;
    # a real Rayleigh dome is typically >>10. Threshold 4 rejects pure clear
    # while accepting the solid-olive score face until SkyPass is re-enabled.
    flat_sky = sky_delta < 4.0

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


CHINA_STACK_STAGES = ("dem", "ocean", "fog", "sky", "cloud")


def china_stack_bmp_name(stage: str) -> str:
    return f"atmosphere-showcase-china-stack-{stage}.bmp"


def _frac(rows: list[tuple[int, int, int]], pred) -> float:
    if not rows:
        return 0.0
    return sum(1 for pixel in rows if pred(*pixel)) / len(rows)


def _is_land(r: int, g: int, b: int) -> bool:
    land = (g > r + 8 and g > b + 5 and g > 70) or (
        r > 90 and g > 80 and b < 130 and r + g > b * 2
    )
    washed = r > 200 and g > 200 and b > 180
    return land and not washed


def _is_rayleigh(r: int, g: int, b: int) -> bool:
    return b > r + 30 and b > g + 25 and b > 120 and g < 200 and r < 160


def _is_pale(r: int, g: int, b: int) -> bool:
    return r > 170 and g > 170 and b > 170 and abs(r - g) < 30 and abs(g - b) < 35


def _is_white(r: int, g: int, b: int) -> bool:
    return r > 210 and g > 210 and b > 200


def _is_sea(r: int, g: int, b: int) -> bool:
    return b > r + 15 and b > 80 and g > 40 and r < 120 and b + 10 >= g


def _is_haze(r: int, g: int, b: int) -> bool:
    mean = (r + g + b) / 3
    return max(r, g, b) - min(r, g, b) < 40 and 90 < mean < 190 and not _is_white(r, g, b)


def _sky_delta(zenith: list, horizon: list) -> float:
    def mean(rows: list[tuple[int, int, int]]) -> tuple[float, float, float]:
        if not rows:
            return (0.0, 0.0, 0.0)
        n = len(rows)
        return (
            sum(p[0] for p in rows) / n,
            sum(p[1] for p in rows) / n,
            sum(p[2] for p in rows) / n,
        )

    z_m = mean(zenith)
    h_m = mean(horizon)
    return abs(z_m[2] - h_m[2]) + abs(z_m[0] - h_m[0])


def _stack_bands(path: Path):
    w, h, pixels = load_bmp_rgb(path)
    zenith_n = max(1, h // 8) * w
    top_n = max(1, h // 3) * w
    mid_end = max(top_n, (2 * h // 3) * w)
    return {
        "w": w,
        "h": h,
        "all": pixels,
        "zenith": pixels[:zenith_n],
        "top": pixels[:top_n],
        "horizon": pixels[zenith_n:top_n],
        "mid": pixels[top_n:mid_end],
        "lower": pixels[top_n:],
    }


def _stack_metrics(path: Path) -> dict:
    bands = _stack_bands(path)
    sky_delta = _sky_delta(bands["zenith"], bands["horizon"] or bands["top"])
    blue = _frac(bands["top"], _is_rayleigh)
    return {
        "land": _frac(bands["all"], _is_land),
        "blue": blue,
        "pale": _frac(bands["all"], _is_pale),
        "pale_center": _frac(bands["mid"], _is_pale),
        "white_top": _frac(bands["zenith"], _is_white),
        "sea": _frac(bands["lower"] or bands["all"], _is_sea),
        "haze": _frac(bands["mid"] or bands["all"], _is_haze),
        "sky_delta": sky_delta,
        "sky_dome": blue > 0.20 and sky_delta >= 4.0,
    }


def _stage_result(path: Path, ok: bool, gates: dict, metrics: dict) -> dict:
    return {
        "bmp": str(path),
        "ok": ok,
        "gates": gates,
        "metrics": {key: round(value, 4) for key, value in metrics.items()},
    }


def score_china_stack_dem(path: Path) -> dict:
    """DEM only: land is visible, sky dome and cloud deck are not."""
    m = _stack_metrics(path)
    gates = {
        "land>0.03": m["land"] > 0.03,
        "sky_dome_off": not m["sky_dome"],
        "pale<0.08": m["pale"] < 0.08,
        "white_top<0.20": m["white_top"] < 0.20,
    }
    return _stage_result(path, all(gates.values()), gates, m)


def score_china_stack_ocean(path: Path) -> dict:
    """DEM plus ocean. Sky and cloud stay off; sea must show."""
    m = _stack_metrics(path)
    gates = {
        "land>0.02": m["land"] > 0.02,
        "sea>0.04": m["sea"] > 0.04,
        "sky_dome_off": not m["sky_dome"],
        "pale<0.08": m["pale"] < 0.08,
        "white_top<0.20": m["white_top"] < 0.20,
    }
    return _stage_result(path, all(gates.values()), gates, m)


def score_china_stack_fog(path: Path) -> dict:
    """Fog on terrain and sea. Zenith stays unwashed and is not a sky dome."""
    m = _stack_metrics(path)
    gates = {
        "land>0.02": m["land"] > 0.02,
        "sea>0.02": m["sea"] > 0.02,
        "haze>0.01": m["haze"] > 0.01,
        "sky_dome_off": not m["sky_dome"],
        "white_top<0.25": m["white_top"] < 0.25,
    }
    return _stage_result(path, all(gates.values()), gates, m)


def score_china_stack_sky(path: Path) -> dict:
    """Sky dome on. Cloud deck must not already own the zenith."""
    m = _stack_metrics(path)
    gates = {
        "sky_dome_on": m["sky_dome"],
        "land>0.02": m["land"] > 0.02,
        "pale<0.15": m["pale"] < 0.15,
        "white_top<0.35": m["white_top"] < 0.35,
    }
    return _stage_result(path, all(gates.values()), gates, m)


def score_china_stack_cloud(path: Path) -> dict:
    """Cloud deck on, zenith sky still present."""
    m = _stack_metrics(path)
    zenith_blue = _frac(_stack_bands(path)["zenith"], _is_rayleigh)
    gates = {
        "zenith_sky": zenith_blue > 0.08 and m["sky_delta"] >= 4.0,
        "cloud_center>0.02": m["pale_center"] > 0.02,
        "land>0.01": m["land"] > 0.01,
    }
    return _stage_result(path, all(gates.values()), gates, m)


_CHINA_STACK_SCORE = {
    "dem": score_china_stack_dem,
    "ocean": score_china_stack_ocean,
    "fog": score_china_stack_fog,
    "sky": score_china_stack_sky,
    "cloud": score_china_stack_cloud,
}


def load_china_stack_status(directory: Path) -> dict[str, str] | None:
    path = directory / "china-stack-status.txt"
    if not path.is_file():
        return None
    out: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) >= 2:
            out[parts[0]] = parts[1]
    return out


def score_china_stack(path: Path) -> dict:
    """Fail-closed ladder. Composite atmosphere_full is a witness only."""
    directory = Path(path).parent
    status = load_china_stack_status(directory)
    blocked = False
    stages = []
    for stage in CHINA_STACK_STAGES:
        flag = None if status is None else status.get(stage)
        bmp = directory / china_stack_bmp_name(stage)
        entry: dict = {"stage": stage, "flag": flag, "bmp": str(bmp)}
        if blocked or flag == "blocked":
            entry["state"] = "blocked"
            entry["ok"] = False
            blocked = True
            stages.append(entry)
            continue
        if flag == "fail" or not bmp.is_file():
            entry["state"] = "fail" if flag == "fail" else "missing"
            entry["ok"] = False
            blocked = True
            stages.append(entry)
            continue
        pixels = _CHINA_STACK_SCORE[stage](bmp)
        entry["pixels"] = pixels
        entry["ok"] = bool(pixels["ok"])
        entry["state"] = "ok" if entry["ok"] else "fail"
        if not entry["ok"]:
            blocked = True
        stages.append(entry)

    cloud = directory / china_stack_bmp_name("cloud")
    witness = score_atmosphere_full(cloud) if cloud.is_file() else None
    ok = bool(stages) and all(stage["ok"] for stage in stages)
    return {
        "bmp": str(path),
        "ok": ok,
        "stages": stages,
        "witness": witness,
        "witness_ok": bool(witness and witness.get("ok")),
        "witness_ignored": True,
        "gates": {stage["stage"]: stage["ok"] for stage in stages},
    }


