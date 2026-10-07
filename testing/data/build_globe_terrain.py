# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Build compact globe DEM + terrain albedo samples under out/data/.

Outputs (shared with Debug/Release via out/data/):
  global_dem.tif      — Float32 equirect DEM (default 720x360)
  global_terrain.tif  — RGB equirect albedo GeoTIFF (product GDAL is GTiff-only)

When china_dem.tif is present, its heights are blended into the China AOI.
Optional network: Earth daymap download re-encoded to GeoTIFF
(--download-blue-marble). Note: shipped GDAL has no JPEG/PNG drivers.

Usage:
  py -3 testing/data/build_globe_terrain.py
  py -3 testing/data/build_globe_terrain.py --download-blue-marble
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import urllib.request
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT_DATA = REPO / "out" / "data"
CACHE = OUT_DATA / "cache" / "globe_terrain_src"
CHINA_DEM = OUT_DATA / "china_dem.tif"
TESTING_CHINA_DEM = REPO / "testing" / "data" / "china" / "china_dem.tif"

# NASA Visible Earth Blue Marble (land_ocean_ice_2048) — small splash albedo.
# Prefer .jpg; historical .png URL 404s on some mirrors.
BLUE_MARBLE_URLS = (
    # Offline-friendly Earth daymap (no clouds), widely mirrored.
    "https://raw.githubusercontent.com/turban/webgl-earth/master/images/"
    "2_no_clouds_4k.jpg",
    "https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57752/"
    "land_ocean_ice_2048.jpg",
    "https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57752/"
    "land_ocean_ice_2048.png",
)

CHINA_BBOX = (73.0, 18.0, 135.0, 54.0)


def _gauss(lon: float, lat: float, cx: float, cy: float, sx: float, sy: float,
           peak: float) -> float:
    dx = (lon - cx) / sx
    dy = (lat - cy) / sy
    return peak * math.exp(-0.5 * (dx * dx + dy * dy))


def synthetic_global_meters(lon: float, lat: float) -> float:
    """Coarse continents on an ocean-dominant splash globe (meters). Ocean ≈ 0."""
    if abs(lat) > 82.0:
        return 80.0

    m = 0.0
    # Compact continents so equirect albedo reads as a blue ocean planet.
    # Eurasia / China
    m += _gauss(lon, lat, 100.0, 40.0, 28.0, 14.0, 900.0)
    m += _gauss(lon, lat, 105.0, 35.0, 14.0, 9.0, 1600.0)
    m += _gauss(lon, lat, 30.0, 50.0, 18.0, 10.0, 450.0)
    # Himalaya / Tibet
    m += _gauss(lon, lat, 88.0, 30.0, 9.0, 4.0, 2800.0)
    # Africa
    m += _gauss(lon, lat, 20.0, 8.0, 12.0, 22.0, 650.0)
    # North America
    m += _gauss(lon, lat, -100.0, 45.0, 20.0, 12.0, 700.0)
    m += _gauss(lon, lat, -110.0, 40.0, 6.0, 10.0, 1400.0)
    # South America
    m += _gauss(lon, lat, -60.0, -12.0, 10.0, 18.0, 550.0)
    m += _gauss(lon, lat, -70.0, -20.0, 3.5, 20.0, 2000.0)
    # Australia
    m += _gauss(lon, lat, 135.0, -25.0, 12.0, 9.0, 400.0)
    # Greenland / Antarctica rim
    m += _gauss(lon, lat, -40.0, 72.0, 10.0, 6.0, 1400.0)
    m += _gauss(lon, lat, 0.0, -80.0, 40.0, 6.0, 1800.0)

    # Carve ocean basins so open water dominates the sphere.
    m -= _gauss(lon, lat, -150.0, 0.0, 55.0, 42.0, 1200.0)
    m -= _gauss(lon, lat, -30.0, 15.0, 40.0, 28.0, 900.0)
    m -= _gauss(lon, lat, 60.0, -20.0, 35.0, 22.0, 800.0)
    m -= _gauss(lon, lat, -40.0, -40.0, 45.0, 28.0, 700.0)
    m -= _gauss(lon, lat, 160.0, 20.0, 40.0, 25.0, 700.0)
    return max(0.0, min(8800.0, m))


def write_geotiff_float32(path: Path, cols: int, rows: int, minx: float,
                          maxy: float, dx: float, dy: float,
                          heights: list[float]) -> None:
    assert len(heights) == cols * rows
    raw = b"".join(struct.pack("<f", h) for h in heights)
    ifd_count = 13
    header_size = 8
    ifd_size = 2 + ifd_count * 12 + 4
    ifd_offset = header_size
    extra_offset = ifd_offset + ifd_size
    pixel_scale = struct.pack("<3d", dx, dy, 0.0)
    tiepoint = struct.pack("<6d", 0.0, 0.0, 0.0, minx, maxy, 0.0)
    geokeys = struct.pack("<4H", 1, 1, 0, 3)
    geokeys += struct.pack("<4H", 1024, 0, 1, 2)
    geokeys += struct.pack("<4H", 1025, 0, 1, 1)
    geokeys += struct.pack("<4H", 2048, 0, 1, 4326)
    blocks = [
        (33550, 12, 3, extra_offset),
        (33922, 12, 6, extra_offset + 24),
        (34735, 3, 16, extra_offset + 72),
    ]
    data_offset = extra_offset + 24 + 48 + 32
    entries = [
        (256, 3, 1, cols),
        (257, 3, 1, rows),
        (258, 3, 1, 32),
        (259, 3, 1, 1),
        (262, 3, 1, 1),
        (273, 4, 1, data_offset),
        (277, 3, 1, 1),
        (278, 3, 1, rows),
        (279, 4, 1, len(raw)),
        (339, 3, 1, 3),
        blocks[0],
        blocks[1],
        blocks[2],
    ]
    buf = bytearray()
    buf += b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset)
    buf += struct.pack("<H", ifd_count)
    for tag, typ, count, val in entries:
        buf += struct.pack("<HHII", tag, typ, count, val)
    buf += struct.pack("<I", 0)
    while len(buf) < extra_offset:
        buf += b"\x00"
    buf += pixel_scale + tiepoint + geokeys
    while len(buf) < data_offset:
        buf += b"\x00"
    buf += raw
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(buf))


def try_sample_china_dem(path: Path, lon: float, lat: float) -> float | None:
    """Best-effort GDAL sample; returns None if unavailable."""
    try:
        from osgeo import gdal  # type: ignore
    except Exception:
        return None
    if not path.is_file():
        return None
    ds = gdal.Open(str(path))
    if ds is None:
        return None
    gt = ds.GetGeoTransform()
    band = ds.GetRasterBand(1)
    # Inverse geotransform (north-up assumed).
    inv = gdal.InvGeoTransform(gt)
    if inv is None:
        px = (lon - gt[0]) / gt[1]
        py = (lat - gt[3]) / gt[5]
    else:
        px, py = gdal.ApplyGeoTransform(inv, lon, lat)
    if px < 0 or py < 0 or px >= ds.RasterXSize or py >= ds.RasterYSize:
        return None
    arr = band.ReadAsArray(int(px), int(py), 1, 1)
    if arr is None:
        return None
    v = float(arr[0, 0])
    nodata = band.GetNoDataValue()
    if nodata is not None and abs(v - float(nodata)) < 1e-3:
        return None
    if v < -500.0:
        return None
    return max(0.0, v)


def build_global_dem(cols: int, rows: int, out: Path) -> None:
    dx = 360.0 / cols
    dy = 180.0 / rows
    china = CHINA_DEM if CHINA_DEM.is_file() else (
        TESTING_CHINA_DEM if TESTING_CHINA_DEM.is_file() else None)
    heights: list[float] = []
    for r in range(rows):
        lat = 90.0 - (r + 0.5) * dy
        for c in range(cols):
            lon = -180.0 + (c + 0.5) * dx
            h = synthetic_global_meters(lon, lat)
            if china is not None:
                minx, miny, maxx, maxy = CHINA_BBOX
                if minx <= lon <= maxx and miny <= lat <= maxy:
                    ch = try_sample_china_dem(china, lon, lat)
                    if ch is not None:
                        # Soft blend near China bbox edge.
                        u = (lon - minx) / (maxx - minx)
                        v = (lat - miny) / (maxy - miny)
                        edge = min(u, 1.0 - u, v, 1.0 - v) / 0.08
                        fade = 0.0 if edge <= 0 else (
                            1.0 if edge >= 1 else edge * edge * (3 - 2 * edge))
                        h = h * (1.0 - fade) + ch * fade
            heights.append(h)
    write_geotiff_float32(out, cols, rows, -180.0, 90.0, dx, dy, heights)
    print(f"OK global DEM → {out} ({out.stat().st_size} bytes)", flush=True)


def terrain_rgba(h: float, lat: float) -> tuple[int, int, int, int]:
    """Generic ocean-blue planet albedo; muted land (no neon / chrome peaks)."""
    # Deep / mid ocean — dominant look for space and near-earth globe.
    if h < 1.0:
        t = max(0.0, min(1.0, (abs(lat) / 75.0)))
        r = int(6 + 8 * t)
        g = int(42 + 18 * t)
        b = int(110 + 28 * (1.0 - t * 0.25))
        return r, g, b, 0
    # Coastal shallows stay blue-green so coasts do not flash chartreuse.
    if h < 80.0:
        return 28, 88, 78, 255
    if h < 400.0:
        return 62, 108, 58, 255
    if h < 1200.0:
        return 96, 112, 62, 255
    if h < 2800.0:
        return 128, 118, 78, 255
    if h < 4500.0:
        return 148, 132, 102, 255
    # High peaks: taupe, not near-white (white + Lambert → chrome glare).
    if abs(lat) > 62.0 or h > 5500.0:
        return 168, 170, 172, 255
    return 155, 145, 128, 255


def write_png_rgba(path: Path, cols: int, rows: int, rgba: bytes) -> None:
    assert len(rgba) == cols * rows * 4

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = b"".join(b"\x00" + rgba[y * cols * 4:(y + 1) * cols * 4]
                   for y in range(rows))
    ihdr = struct.pack(">IIBBBBB", cols, rows, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(
        b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def write_geotiff_rgb8(path: Path, cols: int, rows: int, rgb: bytes) -> None:
    """Write chunky RGB8 GeoTIFF (EPSG:4326 equirect). Sorted IFD tags."""
    assert len(rgb) == cols * rows * 3
    dx = 360.0 / cols
    dy = 180.0 / rows
    minx, maxy = -180.0, 90.0
    ifd_count = 14
    header_size = 8
    ifd_size = 2 + ifd_count * 12 + 4
    ifd_offset = header_size
    extra_offset = ifd_offset + ifd_size
    pixel_scale = struct.pack("<3d", dx, dy, 0.0)
    tiepoint = struct.pack("<6d", 0.0, 0.0, 0.0, minx, maxy, 0.0)
    geokeys = struct.pack("<4H", 1, 1, 0, 3)
    geokeys += struct.pack("<4H", 1024, 0, 1, 2)
    geokeys += struct.pack("<4H", 1025, 0, 1, 1)
    geokeys += struct.pack("<4H", 2048, 0, 1, 4326)
    bits = struct.pack("<3H", 8, 8, 8)
    bits_off = extra_offset + 24 + 48 + 32
    data_offset = bits_off + 6
    if data_offset % 2:
        data_offset += 1
    # Tags must be ascending for libtiff.
    entries = [
        (256, 3, 1, cols),
        (257, 3, 1, rows),
        (258, 3, 3, bits_off),
        (259, 3, 1, 1),
        (262, 3, 1, 2),
        (273, 4, 1, data_offset),
        (277, 3, 1, 3),
        (278, 3, 1, rows),
        (279, 4, 1, len(rgb)),
        (284, 3, 1, 1),
        (339, 3, 1, 1),
        (33550, 12, 3, extra_offset),
        (33922, 12, 6, extra_offset + 24),
        (34735, 3, 16, extra_offset + 72),
    ]
    buf = bytearray()
    buf += b"II" + struct.pack("<H", 42) + struct.pack("<I", ifd_offset)
    buf += struct.pack("<H", ifd_count)
    for tag, typ, count, val in entries:
        buf += struct.pack("<HHII", tag, typ, count, val)
    buf += struct.pack("<I", 0)
    while len(buf) < extra_offset:
        buf += b"\x00"
    buf += pixel_scale + tiepoint + geokeys + bits
    while len(buf) < data_offset:
        buf += b"\x00"
    buf += rgb
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(buf))


def build_terrain_from_dem(dem: Path, cols: int, rows: int, out: Path) -> None:
    china = CHINA_DEM if CHINA_DEM.is_file() else None
    rgb = bytearray(cols * rows * 3)
    for r in range(rows):
        lat = 90.0 - (r + 0.5) * (180.0 / rows)
        for c in range(cols):
            lon = -180.0 + (c + 0.5) * (360.0 / cols)
            h = synthetic_global_meters(lon, lat)
            if china is not None:
                minx, miny, maxx, maxy = CHINA_BBOX
                if minx <= lon <= maxx and miny <= lat <= maxy:
                    ch = try_sample_china_dem(china, lon, lat)
                    if ch is not None:
                        h = ch
            if dem.is_file():
                gh = try_sample_china_dem(dem, lon, lat)
                if gh is not None:
                    h = gh
            rr, gg, bb, _aa = terrain_rgba(h, lat)
            i = (r * cols + c) * 3
            rgb[i] = rr
            rgb[i + 1] = gg
            rgb[i + 2] = bb
    target = out.with_suffix(".tif")
    write_geotiff_rgb8(target, cols, rows, bytes(rgb))
    print(f"OK global terrain → {target} ({target.stat().st_size} bytes)",
          flush=True)


def _gdal_translate_to_tif(src: Path, dst: Path) -> bool:
    """Best-effort: use shipped gdal_translate when present."""
    candidates = [
        REPO / "out" / "third_party" / "bin" / "gdal_translate.exe",
        REPO / "third_party" / ".install" / "bin" / "gdal_translate.exe",
    ]
    for exe in candidates:
        if not exe.is_file():
            continue
        import subprocess
        try:
            subprocess.run(
                [str(exe), "-q", "-of", "GTiff", "-co", "COMPRESS=LZW",
                 "-a_srs", "EPSG:4326", "-a_ullr", "-180", "90", "180", "-90",
                 str(src), str(dst)],
                check=True, capture_output=True, timeout=120)
            return dst.is_file()
        except Exception as exc:
            print(f"WARN: gdal_translate failed: {exc}", flush=True)
    return False


def download_blue_marble(out: Path) -> bool:
    """Download Earth RGB; write global_terrain.tif (GTiff-only GDAL)."""
    CACHE.mkdir(parents=True, exist_ok=True)
    for url in BLUE_MARBLE_URLS:
        dest = CACHE / Path(url.split("?")[0]).name
        try:
            print(f"Downloading Blue Marble → {dest}", flush=True)
            req = urllib.request.Request(
                url, headers={"User-Agent": "SmartGIS-globe-terrain/1.0"})
            with urllib.request.urlopen(req, timeout=120) as resp:
                data = resp.read()
            dest.write_bytes(data)
        except Exception as exc:
            print(f"WARN: download failed ({url}): {exc}", flush=True)
            continue
        out.parent.mkdir(parents=True, exist_ok=True)
        target = out.with_suffix(".tif")
        try:
            from PIL import Image  # type: ignore
            im = Image.open(dest).convert("RGB")
            if im.width > 2048:
                nh = max(256, int(round(im.height * (2048.0 / im.width))))
                im = im.resize((2048, nh), Image.Resampling.LANCZOS)
            write_geotiff_rgb8(target, im.width, im.height, im.tobytes())
            # Optional LZW rewrite via shipped gdal_translate.
            clean = CACHE / "global_terrain_clean.tif"
            if _gdal_translate_to_tif(target, clean):
                target.write_bytes(clean.read_bytes())
            print(f"OK Blue Marble albedo → {target} ({target.stat().st_size} bytes)",
                  flush=True)
            return True
        except Exception as exc:
            print(f"WARN: encode failed ({exc})", flush=True)
            continue
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dem-cols", type=int, default=720)
    ap.add_argument("--dem-rows", type=int, default=360)
    ap.add_argument("--tex-cols", type=int, default=1024)
    ap.add_argument("--tex-rows", type=int, default=512)
    ap.add_argument("--download-blue-marble", action="store_true",
                    help="Prefer Earth daymap GeoTIFF as global_terrain.tif")
    ap.add_argument("--out-dir", type=Path, default=OUT_DATA)
    args = ap.parse_args()

    dem_out = args.out_dir / "global_dem.tif"
    tex_out = args.out_dir / "global_terrain.tif"
    build_global_dem(args.dem_cols, args.dem_rows, dem_out)
    if args.download_blue_marble and download_blue_marble(tex_out):
        return 0
    build_terrain_from_dem(dem_out, args.tex_cols, args.tex_rows, tex_out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
