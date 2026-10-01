# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Load BMP pixels as RGB tuples (top-down row order)."""

from __future__ import annotations

import struct
from pathlib import Path


def load_bmp_rgb(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24 and bpp != 32:
        raise ValueError(f"unsupported bpp={bpp}")
    abs_w = abs(w)
    abs_h = abs(h)
    row_bytes = ((abs_w * (bpp // 8) + 3) // 4) * 4
    top_down = h < 0
    pixels: list[tuple[int, int, int]] = []
    for y in range(abs_h):
        src_y = y if top_down else (abs_h - 1 - y)
        row = offset + src_y * row_bytes
        for x in range(abs_w):
            i = row + x * (bpp // 8)
            b, g, r = data[i], data[i + 1], data[i + 2]
            pixels.append((r, g, b))
    return abs_w, abs_h, pixels


def pixel_diff_frac(
    path_a: Path,
    path_b: Path,
    *,
    thresh: int = 12,
) -> float:
    """Fraction of pixels whose RGB channel delta exceeds |thresh|.

    Size mismatch counts as 1.0 (full change). Used by OS zoom gates
    (e.g. legacy.browse.2d unidirectional wheel_burst before/after).
    """
    wa, ha, pa = load_bmp_rgb(Path(path_a))
    wb, hb, pb = load_bmp_rgb(Path(path_b))
    if wa != wb or ha != hb or not pa:
        return 1.0 if (wa, ha) != (wb, hb) else 0.0
    changed = 0
    for a, b in zip(pa, pb):
        if (
            abs(a[0] - b[0]) > thresh
            or abs(a[1] - b[1]) > thresh
            or abs(a[2] - b[2]) > thresh
        ):
            changed += 1
    return changed / float(len(pa))
