# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Convert harness BMP captures to PNG for Agent Read / visual review."""

from __future__ import annotations

import struct
import subprocess
from pathlib import Path


def inspect_png_path(bmp: Path) -> Path:
    """Sibling ``*.inspect.png`` next to the BMP stem."""
    return bmp.with_suffix("").with_name(bmp.stem + ".inspect.png")


def bmp_to_inspect_png(bmp: Path, *, dest: Path | None = None) -> Path:
    """Write ``*.inspect.png`` from a BMP. Prefer Pillow; fall back to magick.

    Returns the PNG path. Raises ``FileNotFoundError`` / ``RuntimeError`` on failure.
    """
    bmp = bmp.resolve()
    if not bmp.is_file():
        raise FileNotFoundError(f"missing BMP: {bmp}")
    out = dest.resolve() if dest is not None else inspect_png_path(bmp)
    out.parent.mkdir(parents=True, exist_ok=True)

    try:
        from PIL import Image  # type: ignore[import-untyped]
    except ImportError:
        Image = None  # type: ignore[assignment,misc]

    if Image is not None:
        with Image.open(bmp) as im:
            # Flatten palette / odd BMP modes to RGB for stable Agent Read.
            rgb = im.convert("RGB")
            rgb.save(out, format="PNG")
        return out

    magick = _find_magick()
    if magick is not None:
        proc = subprocess.run(
            [magick, str(bmp), str(out)],
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode == 0 and out.is_file():
            return out
        raise RuntimeError(
            f"magick convert failed rc={proc.returncode}: {proc.stderr.strip()}"
        )

    # Last resort: uncompressed BI_RGB 24-bit BMP → PNG via stdlib zlib.
    _bmp24_to_png_stdlib(bmp, out)
    return out


def _find_magick() -> str | None:
    for name in ("magick", "convert"):
        try:
            proc = subprocess.run(
                [name, "-version"],
                capture_output=True,
                text=True,
                check=False,
            )
            if proc.returncode == 0:
                return name
        except OSError:
            continue
    return None


def _bmp24_to_png_stdlib(bmp: Path, out: Path) -> None:
    """Minimal BI_RGB 24bpp bottom-up BMP → RGB PNG (no Pillow / magick)."""
    import zlib

    data = bmp.read_bytes()
    if len(data) < 54 or data[0:2] != b"BM":
        raise RuntimeError(f"not a BMP or too small: {bmp}")
    pixel_off = struct.unpack_from("<I", data, 10)[0]
    dib = struct.unpack_from("<IiiHHIIiiII", data, 14)
    # header_size, width, height, planes, bpp, compression, ...
    width, height = int(dib[1]), int(dib[2])
    bpp, compression = int(dib[4]), int(dib[5])
    if compression != 0 or bpp != 24 or width <= 0:
        raise RuntimeError(
            f"stdlib BMP→PNG needs BI_RGB 24bpp (got bpp={bpp} comp={compression}); "
            "install Pillow"
        )
    bottom_up = height > 0
    height = abs(height)
    row_raw = ((width * 3 + 3) // 4) * 4
    need = pixel_off + row_raw * height
    if len(data) < need:
        raise RuntimeError(f"BMP truncated: {bmp}")

    rows: list[bytes] = []
    for y in range(height):
        src_y = (height - 1 - y) if bottom_up else y
        off = pixel_off + src_y * row_raw
        row = bytearray()
        for x in range(width):
            b, g, r = data[off + x * 3 : off + x * 3 + 3]
            row.extend((r, g, b))
        rows.append(bytes(row))

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)  # 8-bit RGB
    raw = b"".join(b"\x00" + r for r in rows)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    out.write_bytes(png)
