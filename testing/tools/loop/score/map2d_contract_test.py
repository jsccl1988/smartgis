# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Cross-check: one still cannot satisfy both China design contracts."""

from __future__ import annotations

from .map2d_contract import evaluate_contract, measure_bands


def _fill(w: int, h: int, color: tuple[int, int, int]) -> list[tuple[int, int, int]]:
    return [color] * (w * h)


def _paint(
    pixels: list[tuple[int, int, int]],
    w: int,
    x0: int,
    y0: int,
    x1: int,
    y1: int,
    color: tuple[int, int, int],
) -> None:
    for y in range(y0, y1):
        row = y * w
        for x in range(x0, x1):
            pixels[row + x] = color


def _dem_still() -> list[tuple[int, int, int]]:
    w, h = 1280, 720
    ocean = (170, 211, 223)
    jet_low = (30, 90, 180)
    jet_high = (210, 40, 30)
    shade = (130, 128, 125)
    gold = (224, 192, 106)
    casing = (138, 112, 64)
    admin = (44, 36, 24)
    river = (100, 160, 208)
    pixels = _fill(w, h, jet_low)
    _paint(pixels, w, 0, 0, 380, h, ocean)
    _paint(pixels, w, 400, 40, 900, 360, jet_high)
    _paint(pixels, w, 400, 400, 1100, 680, shade)
    _paint(pixels, w, 420, 80, 428, 640, casing)
    _paint(pixels, w, 430, 80, 436, 640, gold)
    _paint(pixels, w, 500, 100, 506, 600, admin)
    _paint(pixels, w, 560, 120, 566, 620, river)
    return pixels


def _carto_still(*, admin: bool) -> list[tuple[int, int, int]]:
    w, h = 1280, 720
    ocean = (170, 211, 223)
    cream = (245, 243, 233)
    gold = (224, 192, 106)
    casing = (138, 112, 64)
    river = (100, 160, 208)
    pixels = _fill(w, h, cream)
    _paint(pixels, w, 0, 0, 460, h, ocean)
    _paint(pixels, w, 500, 80, 508, 660, casing)
    _paint(pixels, w, 510, 80, 516, 660, gold)
    _paint(pixels, w, 600, 100, 606, 640, river)
    if admin:
        _paint(pixels, w, 700, 90, 706, 640, (44, 36, 24))
    return pixels


def _expect(pixels: list[tuple[int, int, int]], contract: str, ok: bool) -> None:
    scored = evaluate_contract(measure_bands(1280, 720, pixels), contract)
    if scored["ok"] is not ok:
        failed = [name for name, passed in scored["gates"].items() if not passed]
        raise SystemExit(f"{contract} ok={scored['ok']} want {ok} failed={failed}")


def main() -> None:
    dem = _dem_still()
    carto = _carto_still(admin=True)
    align = _carto_still(admin=False)
    _expect(dem, "map2d_china_dem", True)
    _expect(dem, "map2d_china_carto", False)
    _expect(carto, "map2d_china_carto", True)
    _expect(carto, "map2d_china_dem", False)
    _expect(align, "map2d_align", True)
    _expect(align, "map2d_china_carto", False)
    _expect(carto, "map2d_align", True)
    print("map2d_contract_test ok")


if __name__ == "__main__":
    main()
