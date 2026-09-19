#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build testing/data/china_dem.tif from a public real DEM (default).

Default source: AWS Open Data / Mapzen Terrain GeoTIFF tiles
  https://s3.amazonaws.com/elevation-tiles-prod/geotiff/{z}/{x}/{y}.tif
  (SRTM / GMTED / NED / ETOPO derivatives; see LICENSE notes).

Pipeline:
  1) Download WebMercator tiles covering China bbox (cached under
     out/china_dem_src/).
  2) gdalbuildvrt + gdalwarp → EPSG:4326 Float32 grid.
  3) Optional national outline cutline so ocean = NoData/0 (matches 2D map).

Fallback: --source synthetic (physiography model; no network).

Usage:
  py -3 testing/data/build_china_dem.py
  py -3 testing/data/build_china_dem.py --zoom 6 --cols 720 --rows 450
  py -3 testing/data/build_china_dem.py --source synthetic

Requires: Python 3.10+, urllib; GDAL CLI from third_party/.install/bin
(or third_party/gdal_sdk/bin).
"""

from __future__ import annotations

import argparse
import json
import math
import os
import struct
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_OUT = Path(__file__).resolve().parent / "china_dem.tif"
CACHE = REPO / "out" / "china_dem_src"
CHINA_BBOX = (73.0, 17.5, 135.0, 54.0)  # minx, miny, maxx, maxy
TILE_URL = "https://s3.amazonaws.com/elevation-tiles-prod/geotiff/{z}/{x}/{y}.tif"
# Mirror (optional) — same Open Data bucket via cloudfront-style hosts if needed.
TILE_URL_MIRRORS = (
    TILE_URL,
    "https://elevation-tiles-prod.s3.amazonaws.com/geotiff/{z}/{x}/{y}.tif",
)


def find_gdal_bin() -> Path:
    candidates = [
        REPO / "third_party" / ".install" / "bin",
        REPO / "third_party" / "gdal_sdk" / "bin",
        REPO / "out" / "third_party" / ".install" / "bin",
    ]
    for d in candidates:
        if (d / "gdalwarp.exe").is_file() or (d / "gdalwarp").is_file():
            return d
    raise FileNotFoundError(
        "gdalwarp not found under third_party/.install/bin — build/install GDAL first")


def gdal_exe(bin_dir: Path, name: str) -> str:
    win = bin_dir / f"{name}.exe"
    if win.is_file():
        return str(win)
    unix = bin_dir / name
    if unix.is_file():
        return str(unix)
    raise FileNotFoundError(f"{name} not in {bin_dir}")


def run_gdal(bin_dir: Path, name: str, args: list[str]) -> None:
    cmd = [gdal_exe(bin_dir, name), *args]
    print("+", " ".join(cmd), flush=True)
    env = os.environ.copy()
    # Prefer sibling DLLs next to the tools.
    env["PATH"] = str(bin_dir) + os.pathsep + env.get("PATH", "")
    subprocess.check_call(cmd, env=env)


def lonlat_to_tile(lon: float, lat: float, z: int) -> tuple[int, int]:
    n = 2**z
    x = int(math.floor((lon + 180.0) / 360.0 * n))
    lat = max(min(lat, 85.05112878), -85.05112878)
    lat_rad = math.radians(lat)
    y = int(
        math.floor((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) /
                    math.pi) / 2.0 * n))
    x = max(0, min(n - 1, x))
    y = max(0, min(n - 1, y))
    return x, y


def tiles_for_bbox(bbox: tuple[float, float, float, float],
                   z: int) -> list[tuple[int, int]]:
    minx, miny, maxx, maxy = bbox
    x0, y_north = lonlat_to_tile(minx, maxy, z)
    x1, y_south = lonlat_to_tile(maxx, miny, z)
    tiles: list[tuple[int, int]] = []
    for x in range(min(x0, x1), max(x0, x1) + 1):
        for y in range(min(y_north, y_south), max(y_north, y_south) + 1):
            tiles.append((x, y))
    return tiles


def download_tile(z: int, x: int, y: int, dest: Path) -> None:
    if dest.exists() and dest.stat().st_size > 1000:
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    last: Exception | None = None
    for tmpl in TILE_URL_MIRRORS:
        url = tmpl.format(z=z, x=x, y=y)
        try:
            print(f"GET {url}", flush=True)
            urllib.request.urlretrieve(url, dest)
            if dest.stat().st_size < 100:
                raise RuntimeError("tiny response")
            return
        except Exception as e:  # noqa: BLE001
            last = e
            print(f"  fail: {e}", flush=True)
            if dest.exists():
                dest.unlink(missing_ok=True)
    raise RuntimeError(f"tile z={z} x={x} y={y}: {last}")


def write_outline_geojson(rings_path: Path, out_geojson: Path) -> bool:
    """Write a MultiPolygon FeatureCollection for gdalwarp -cutline."""
    if not rings_path.exists():
        return False
    data = json.loads(rings_path.read_text(encoding="utf-8"))
    features_in = data.get("features") or []
    polys: list = []
    for feat in features_in:
        geom = feat.get("geometry") or {}
        gtype = geom.get("type")
        coords = geom.get("coordinates")
        if not coords:
            continue
        if gtype == "Polygon":
            polys.append(coords)
        elif gtype == "MultiPolygon":
            polys.extend(coords)
    if not polys:
        return False
    fc = {
        "type": "FeatureCollection",
        "name": "china_outline",
        "crs": {
            "type": "name",
            "properties": {
                "name": "urn:ogc:def:crs:OGC:1.3:CRS84"
            }
        },
        "features": [{
            "type": "Feature",
            "properties": {"name": "china"},
            "geometry": {
                "type": "MultiPolygon",
                "coordinates": polys
            },
        }],
    }
    out_geojson.parent.mkdir(parents=True, exist_ok=True)
    out_geojson.write_text(json.dumps(fc, ensure_ascii=False), encoding="utf-8")
    print(f"cutline: {out_geojson} ({len(polys)} polygons)", flush=True)
    return True


def build_real(out: Path, zoom: int, cols: int, rows: int,
               apply_cutline: bool) -> int:
    bin_dir = find_gdal_bin()
    CACHE.mkdir(parents=True, exist_ok=True)
    tiles = tiles_for_bbox(CHINA_BBOX, zoom)
    print(f"tiles z={zoom}: {len(tiles)}", flush=True)
    paths: list[Path] = []
    for i, (x, y) in enumerate(tiles):
        dest = CACHE / f"z{zoom}" / f"{x}_{y}.tif"
        download_tile(zoom, x, y, dest)
        paths.append(dest)
        if (i + 1) % 10 == 0 or i + 1 == len(tiles):
            print(f"  downloaded {i + 1}/{len(tiles)}", flush=True)

    list_file = CACHE / f"tiles_z{zoom}.txt"
    list_file.write_text("\n".join(str(p) for p in paths) + "\n", encoding="utf-8")
    vrt = CACHE / f"china_z{zoom}.vrt"
    run_gdal(bin_dir, "gdalbuildvrt", ["-input_file_list", str(list_file), str(vrt)])

    minx, miny, maxx, maxy = CHINA_BBOX
    warped = CACHE / "china_dem_warp.tif"
    warp_args = [
        "-t_srs", "EPSG:4326",
        "-te", str(minx), str(miny), str(maxx), str(maxy),
        "-ts", str(cols), str(rows),
        "-r", "bilinear",
        "-ot", "Float32",
        "-dstnodata", "-32768",
        "-co", "COMPRESS=LZW",
        "-co", "TILED=YES",
        "-overwrite",
        str(vrt),
        str(warped),
    ]
    run_gdal(bin_dir, "gdalwarp", warp_args)

    final_src = warped
    if apply_cutline:
        outline_candidates = [
            Path(__file__).resolve().parent / "_china_100000_full.json",
            REPO / "out" / "china_city_src" / "china_full.json",
        ]
        cutline = CACHE / "china_outline.geojson"
        wrote = False
        for cand in outline_candidates:
            if write_outline_geojson(cand, cutline):
                wrote = True
                break
        if wrote:
            masked = CACHE / "china_dem_masked.tif"
            run_gdal(bin_dir, "gdalwarp", [
                "-cutline", str(cutline),
                "-dstnodata", "0",
                "-ot", "Float32",
                "-co", "COMPRESS=LZW",
                "-co", "TILED=YES",
                "-overwrite",
                str(warped),
                str(masked),
            ])
            # Replace remaining nodata with 0 for DemHeightField (treats < -1000).
            final_src = CACHE / "china_dem_zeroed.tif"
            run_gdal(bin_dir, "gdal_translate", [
                "-a_nodata", "none",
                "-ot", "Float32",
                "-co", "COMPRESS=LZW",
                str(masked),
                str(final_src),
            ])
        else:
            print("WARN: no outline JSON; skipping cutline", flush=True)

    out.parent.mkdir(parents=True, exist_ok=True)
    run_gdal(bin_dir, "gdal_translate", [
        "-ot", "Float32",
        "-co", "COMPRESS=LZW",
        "-co", "TILED=YES",
        str(final_src),
        str(out),
    ])
    run_gdal(bin_dir, "gdalinfo", ["-stats", str(out)])
    print(f"OK real DEM → {out} ({out.stat().st_size} bytes)", flush=True)
    return 0


# --- synthetic fallback (no GDAL / offline) ---------------------------------

def _gauss(lon: float, lat: float, cx: float, cy: float, sx: float, sy: float,
           peak: float) -> float:
    dx = (lon - cx) / sx
    dy = (lat - cy) / sy
    return peak * math.exp(-0.5 * (dx * dx + dy * dy))


def synthetic_meters(lon: float, lat: float) -> float:
    m = 80.0
    m += 2800.0 * math.exp(
        -0.5 * ((lon - 90.0) / 14.0) ** 2 - 0.5 * ((lat - 33.0) / 6.5) ** 2)
    m += _gauss(lon, lat, 86.5, 28.0, 3.8, 1.8, 2200.0)
    m += _gauss(lon, lat, 99.0, 28.5, 2.8, 2.2, 2400.0)
    m += _gauss(lon, lat, 85.0, 42.5, 5.5, 1.5, 2200.0)
    m += _gauss(lon, lat, 107.5, 34.0, 3.5, 1.4, 900.0)
    m += _gauss(lon, lat, 121.0, 23.8, 0.55, 1.1, 2400.0)
    m -= _gauss(lon, lat, 84.0, 40.5, 4.5, 2.2, 2200.0)
    m -= _gauss(lon, lat, 116.5, 33.0, 6.0, 4.5, 400.0)
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
        ("scale", pixel_scale),
        ("tie", tiepoint),
        ("geokey", geokeys),
        ("strip", raw),
    ]
    offsets: dict[str, int] = {}
    cursor = extra_offset
    for name, blob in blocks:
        offsets[name] = cursor
        cursor += len(blob)

    def entry(tag: int, typ: int, count: int, value: int) -> bytes:
        return struct.pack("<HHII", tag, typ, count, value)

    entries = [
        entry(256, 3, 1, cols),
        entry(257, 3, 1, rows),
        entry(258, 3, 1, 32),
        entry(259, 3, 1, 1),
        entry(262, 3, 1, 1),
        entry(273, 4, 1, offsets["strip"]),
        entry(277, 3, 1, 1),
        entry(278, 3, 1, rows),
        entry(279, 4, 1, len(raw)),
        entry(339, 3, 1, 3),
        entry(33550, 12, 3, offsets["scale"]),
        entry(33922, 12, 6, offsets["tie"]),
        entry(34735, 3, len(geokeys) // 2, offsets["geokey"]),
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(b"II")
        f.write(struct.pack("<H", 42))
        f.write(struct.pack("<I", ifd_offset))
        f.write(struct.pack("<H", ifd_count))
        for e in entries:
            f.write(e)
        f.write(struct.pack("<I", 0))
        pos = f.tell()
        if pos < extra_offset:
            f.write(b"\x00" * (extra_offset - pos))
        for _, blob in blocks:
            f.write(blob)


def point_in_ring(px: float, py: float, xs: list[float], ys: list[float]) -> bool:
    n = len(xs)
    inside = False
    j = n - 1
    for i in range(n):
        yi, yj = ys[i], ys[j]
        xi, xj = xs[i], xs[j]
        if ((yi > py) != (yj > py)) and (
                px < (xj - xi) * (py - yi) / ((yj - yi) + 0.0) + xi):
            inside = not inside
        j = i
    return inside


def build_synthetic(out: Path, cols: int, rows: int) -> int:
    outline = Path(__file__).resolve().parent / "_china_100000_full.json"
    rings: list[tuple[list[float], list[float]]] = []
    if outline.exists():
        data = json.loads(outline.read_text(encoding="utf-8"))
        for feat in data.get("features") or []:
            geom = feat.get("geometry") or {}
            coords = geom.get("coordinates")
            gtype = geom.get("type")
            if gtype == "Polygon" and coords:
                xs = [float(c[0]) for c in coords[0]]
                ys = [float(c[1]) for c in coords[0]]
                rings.append((xs, ys))
            elif gtype == "MultiPolygon" and coords:
                for poly in coords:
                    xs = [float(c[0]) for c in poly[0]]
                    ys = [float(c[1]) for c in poly[0]]
                    rings.append((xs, ys))
    minx, miny, maxx, maxy = CHINA_BBOX
    dx = (maxx - minx) / cols
    dy = (maxy - miny) / rows
    heights: list[float] = []
    for row in range(rows):
        lat = maxy - (row + 0.5) * dy
        for col in range(cols):
            lon = minx + (col + 0.5) * dx
            if rings and not any(point_in_ring(lon, lat, xs, ys) for xs, ys in rings):
                heights.append(0.0)
            else:
                heights.append(synthetic_meters(lon, lat))
    write_geotiff_float32(out, cols, rows, minx, maxy, dx, dy, heights)
    print(f"OK synthetic DEM → {out}", flush=True)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--source", choices=("real", "synthetic"), default="real")
    parser.add_argument("--zoom", type=int, default=6,
                        help="AWS terrain tile zoom (5=coarse, 6=default, 7=heavier)")
    parser.add_argument("--cols", type=int, default=720)
    parser.add_argument("--rows", type=int, default=450)
    parser.add_argument("--no-cutline", action="store_true",
                        help="Keep full bbox without national outline mask")
    args = parser.parse_args()
    if args.source == "synthetic":
        return build_synthetic(args.out, args.cols, args.rows)
    try:
        return build_real(args.out, args.zoom, args.cols, args.rows,
                          apply_cutline=not args.no_cutline)
    except Exception as e:  # noqa: BLE001
        print(f"real DEM failed ({e}); falling back to synthetic", flush=True)
        return build_synthetic(args.out, args.cols, args.rows)


if __name__ == "__main__":
    sys.exit(main())
