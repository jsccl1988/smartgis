#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Burn a solid engine-id title strip into engine-*.bmp (pure Python, no GDI)."""

from __future__ import annotations

import struct
from pathlib import Path


def _repo_root() -> Path:
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "build.bat").is_file() and (p / "testing").is_dir():
            return p
        p = p.parent
    raise RuntimeError("repo root not found")


OUT = _repo_root() / "out" / "Debug"

LABELS = {
    "views-map2d": "Views Map2D (Skia/RHI)",
    "legacy-map2d-gdi": "Legacy Map2D (GDI+)",
    "views-scene3d-atmosphere": "Views Scene3D (Atmosphere/FlyCube)",
    "legacy-scene3d-gl": "Legacy Scene3D (OpenGL)",
    "legacy-scene3d-d3d": "Legacy Scene3D (D3D11)",
}

# 5x7 monospace glyphs for A-Z a-z 0-9 | () / + - _ . space
_GLYPHS: dict[str, list[str]] = {
    " ": ["00000"] * 7,
    "|": ["01000", "01000", "01000", "01000", "01000", "01000", "01000"],
    "(": ["00100", "01000", "10000", "10000", "10000", "01000", "00100"],
    ")": ["10000", "01000", "00100", "00100", "00100", "01000", "10000"],
    "/": ["00001", "00010", "00100", "01000", "10000", "00000", "00000"],
    "+": ["00000", "00100", "00100", "11111", "00100", "00100", "00000"],
    "-": ["00000", "00000", "00000", "11111", "00000", "00000", "00000"],
    "_": ["00000", "00000", "00000", "00000", "00000", "00000", "11111"],
    ".": ["00000", "00000", "00000", "00000", "00000", "01100", "01100"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["01110", "10001", "00001", "00110", "00001", "10001", "01110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01110"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10001", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10001", "10101", "10101", "01010"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "a": ["00000", "00000", "01110", "00001", "01111", "10001", "01111"],
    "b": ["10000", "10000", "11110", "10001", "10001", "10001", "11110"],
    "c": ["00000", "00000", "01110", "10000", "10000", "10001", "01110"],
    "d": ["00001", "00001", "01111", "10001", "10001", "10001", "01111"],
    "e": ["00000", "00000", "01110", "10001", "11111", "10000", "01110"],
    "g": ["00000", "00000", "01111", "10001", "01111", "00001", "01110"],
    "h": ["10000", "10000", "11110", "10001", "10001", "10001", "10001"],
    "i": ["00100", "00000", "01100", "00100", "00100", "00100", "01110"],
    "k": ["10000", "10000", "10010", "10100", "11000", "10100", "10010"],
    "l": ["01100", "00100", "00100", "00100", "00100", "00100", "01110"],
    "m": ["00000", "00000", "11010", "10101", "10101", "10001", "10001"],
    "n": ["00000", "00000", "11110", "10001", "10001", "10001", "10001"],
    "o": ["00000", "00000", "01110", "10001", "10001", "10001", "01110"],
    "p": ["00000", "00000", "11110", "10001", "11110", "10000", "10000"],
    "r": ["00000", "00000", "10110", "11001", "10000", "10000", "10000"],
    "s": ["00000", "00000", "01111", "10000", "01110", "00001", "11110"],
    "t": ["01000", "01000", "11100", "01000", "01000", "01001", "00110"],
    "u": ["00000", "00000", "10001", "10001", "10001", "10011", "01101"],
    "v": ["00000", "00000", "10001", "10001", "10001", "01010", "00100"],
    "w": ["00000", "00000", "10001", "10001", "10101", "10101", "01010"],
    "y": ["00000", "00000", "10001", "10001", "01111", "00001", "01110"],
    "3": ["01110", "10001", "00001", "00110", "00001", "10001", "01110"],
}


def _load_pixels(path: Path) -> tuple[int, int, int, bytearray]:
    """Return (w, h, bpp, pixels) for 24bpp BGR or 32bpp BGRA BI_RGB."""
    data = bytearray(path.read_bytes())
    if data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if bpp not in (24, 32) or compression not in (0, 3):
        raise ValueError(f"unsupported BMP bpp={bpp} comp={compression}: {path}")
    abs_h = abs(h)
    row = ((w * (bpp // 8) + 3) // 4) * 4
    pixels = bytearray(data[off : off + row * abs_h])
    return w, h, bpp, pixels


def _save_bgr24(path: Path, w: int, h: int, pixels: bytes) -> None:
    abs_h = abs(h)
    row = ((w * 3 + 3) // 4) * 4
    payload = bytes(pixels)
    if len(payload) != row * abs_h:
        raise ValueError("pixel buffer size mismatch")
    header = bytearray(54)
    header[0:2] = b"BM"
    struct.pack_into("<I", header, 2, 54 + len(payload))
    struct.pack_into("<I", header, 10, 54)
    struct.pack_into("<I", header, 14, 40)
    struct.pack_into("<ii", header, 18, w, h)
    struct.pack_into("<HH", header, 26, 1, 24)
    struct.pack_into("<I", header, 34, len(payload))
    path.write_bytes(bytes(header) + payload)


def _to_bgr24(w: int, h: int, bpp: int, pixels: bytearray) -> bytearray:
    abs_h = abs(h)
    if bpp == 24:
        return pixels
    src_row = ((w * 4 + 3) // 4) * 4
    dst_row = ((w * 3 + 3) // 4) * 4
    out = bytearray(dst_row * abs_h)
    for y in range(abs_h):
        s = y * src_row
        d = y * dst_row
        for x in range(w):
            out[d + x * 3 : d + x * 3 + 3] = pixels[s + x * 4 : s + x * 4 + 3]
    return out


def _put_bgr(pixels: bytearray, w: int, abs_h: int, bottom_up: bool, x: int, y_top: int, bgr: tuple[int, int, int]) -> None:
    if x < 0 or x >= w or y_top < 0 or y_top >= abs_h:
        return
    row = ((w * 3 + 3) // 4) * 4
    file_y = (abs_h - 1 - y_top) if bottom_up else y_top
    i = file_y * row + x * 3
    pixels[i : i + 3] = bytes(bgr)


def _draw_text(pixels: bytearray, w: int, abs_h: int, bottom_up: bool, text: str, x0: int, y0: int, scale: int, bgr: tuple[int, int, int]) -> None:
    cx = x0
    for ch in text:
        glyph = _GLYPHS.get(ch) or _GLYPHS.get(ch.upper()) or _GLYPHS[" "]
        for gy, row_bits in enumerate(glyph):
            for gx, bit in enumerate(row_bits):
                if bit != "1":
                    continue
                for sy in range(scale):
                    for sx in range(scale):
                        _put_bgr(pixels, w, abs_h, bottom_up, cx + gx * scale + sx, y0 + gy * scale + sy, bgr)
        cx += 6 * scale


def annotate(path: Path, engine_id: str, label: str) -> None:
    w, h, bpp, raw = _load_pixels(path)
    pixels = _to_bgr24(w, h, bpp, raw)
    abs_h = abs(h)
    bottom_up = h > 0
    bar_h = 36
    # Solid black bar at visual top.
    for y in range(bar_h):
        for x in range(w):
            _put_bgr(pixels, w, abs_h, bottom_up, x, y, (0, 0, 0))
    # Bright yellow underline for quick eye scan.
    for x in range(w):
        _put_bgr(pixels, w, abs_h, bottom_up, x, bar_h - 2, (0, 255, 255))
        _put_bgr(pixels, w, abs_h, bottom_up, x, bar_h - 1, (0, 255, 255))
    title = f"{engine_id} | {label}"
    _draw_text(pixels, w, abs_h, bottom_up, title, 8, 8, 2, (0, 255, 255))
    _save_bgr24(path, w, h, pixels)
    print("labeled", path.name, "->", title)


def main() -> None:
    for engine_id, label in LABELS.items():
        bmp = OUT / f"engine-{engine_id}.bmp"
        if not bmp.exists():
            print("missing", bmp.name)
            continue
        annotate(bmp, engine_id, label)


if __name__ == "__main__":
    main()
