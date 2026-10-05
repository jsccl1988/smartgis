#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build testing/data/china/china_dem.tif aligned with china_city vectors.

Default source: AWS Open Data / Mapzen Terrain GeoTIFF tiles
  https://s3.amazonaws.com/elevation-tiles-prod/geotiff/{z}/{x}/{y}.tif
  (SRTM / GMTED / NED / ETOPO derivatives; see LICENSE notes).

CRS: EPSG:4326 — same as Natural Earth china_city vectors. Land cutline
prefers the NE admin_1 outline written by build_china_city.py (not DataV
GCJ-02), so rivers/roads/DEM share one land mask.

Pipeline:
  1) Download WebMercator tiles covering China bbox (cached under
     out/data/cache/china_dem_src/).
  2) gdalbuildvrt + gdalwarp → EPSG:4326 Float32 grid.
  3) Optional national outline cutline so ocean = NoData/0 (matches 2D map).

Fallback: --source synthetic (physiography model; no network).

Usage:
  py -3 testing/data/build_china_dem.py --jobs 16
  py -3 testing/data/build_china_city.py --with-dem
  py -3 testing/data/build_china_dem.py --zoom 7 --cols 1536 --rows 960
  py -3 testing/data/build_china_dem.py --zoom 8 --cols 2048 --rows 1280
  py -3 testing/data/build_china_dem.py --source synthetic

Requires: Python 3.10+, urllib; GDAL CLI from third_party/.install/bin.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import threading
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CHINA_DIR = Path(__file__).resolve().parent / "china"
DEFAULT_OUT = CHINA_DIR / "china_dem.tif"
CACHE = REPO / "out" / "data" / "cache" / "china_dem_src"
# Match build_china_city.CHINA_BBOX / content::kChinaLonLatExtent.
CHINA_BBOX = (73.0, 18.0, 135.0, 54.0)  # minx, miny, maxx, maxy
TILE_URL = "https://s3.amazonaws.com/elevation-tiles-prod/geotiff/{z}/{x}/{y}.tif"
TILE_URL_MIRRORS = (
    TILE_URL,
    "https://elevation-tiles-prod.s3.amazonaws.com/geotiff/{z}/{x}/{y}.tif",
)
DEFAULT_JOBS = 12
_print_lock = threading.Lock()


def _log(msg: str) -> None:
    with _print_lock:
        print(msg, flush=True)


def find_gdal_bin() -> Path:
    candidates = [
        REPO / "third_party" / ".install" / "bin",
        REPO / "out" / "third_party" / "bin",
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


def download_tile(z: int, x: int, y: int, dest: Path, *, quiet: bool = False) -> Path:
    if dest.exists() and dest.stat().st_size > 1000:
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    last: Exception | None = None
    for tmpl in TILE_URL_MIRRORS:
        url = tmpl.format(z=z, x=x, y=y)
        try:
            if not quiet:
                _log(f"GET {url}")
            # Write to a sibling temp then rename so parallel retries cannot
            # leave a truncated .tif that looks "cached".
            tmp = dest.with_suffix(dest.suffix + ".part")
            if tmp.exists():
                tmp.unlink(missing_ok=True)
            urllib.request.urlretrieve(url, tmp)
            if tmp.stat().st_size < 100:
                raise RuntimeError("tiny response")
            tmp.replace(dest)
            return dest
        except Exception as e:  # noqa: BLE001
            last = e
            _log(f"  fail {z}/{x}/{y}: {e}")
            for p in (dest, dest.with_suffix(dest.suffix + ".part")):
                if p.exists():
                    p.unlink(missing_ok=True)
    raise RuntimeError(f"tile z={z} x={x} y={y}: {last}")


def download_tiles_parallel(
    z: int, tiles: list[tuple[int, int]], jobs: int
) -> list[Path]:
    """Fetch terrain tiles with a thread pool; skip files already cached."""
    jobs = max(1, min(jobs, len(tiles) or 1))
    work: list[tuple[int, int, Path]] = []
    paths: list[Path] = []
    cached = 0
    for x, y in tiles:
        dest = CACHE / f"z{z}" / f"{x}_{y}.tif"
        paths.append(dest)
        if dest.exists() and dest.stat().st_size > 1000:
            cached += 1
        else:
            work.append((x, y, dest))
    _log(f"tiles z={z}: {len(tiles)} total, {cached} cached, {len(work)} to fetch "
         f"(jobs={jobs})")
    if not work:
        return paths

    done = 0
    errors: list[str] = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futs = {
            pool.submit(download_tile, z, x, y, dest, quiet=True): (x, y)
            for x, y, dest in work
        }
        for fut in as_completed(futs):
            x, y = futs[fut]
            done += 1
            try:
                fut.result()
            except Exception as e:  # noqa: BLE001
                errors.append(f"{x}/{y}: {e}")
            if done % 10 == 0 or done == len(work):
                _log(f"  fetched {done}/{len(work)}")
    if errors:
        raise RuntimeError(
            f"{len(errors)} tile(s) failed, e.g. {errors[0]}"
            + (f" … (+{len(errors) - 1} more)" if len(errors) > 1 else "")
        )
    return paths


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


def ensure_cutline(candidates: list[Path], cutline: Path) -> bool:
    """Normalize NE land outline into cutline path for gdalwarp."""
    for cand in candidates:
        if not cand.exists() or cand.stat().st_size < 100:
            continue
        if cand.resolve() == cutline.resolve():
            return True
        if write_outline_geojson(cand, cutline):
            return True
    return False


def build_real(out: Path, zoom: int, cols: int, rows: int,
               apply_cutline: bool, jobs: int = DEFAULT_JOBS) -> int:
    bin_dir = find_gdal_bin()
    CACHE.mkdir(parents=True, exist_ok=True)
    tiles = tiles_for_bbox(CHINA_BBOX, zoom)
    paths = download_tiles_parallel(zoom, tiles, jobs)

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
        # Prefer Natural Earth land outline (same CRS as china_city vectors).
        # Do not use Aliyun DataV china_full.json (GCJ-02) — misaligns DEM.
        outline_candidates = [
            CHINA_DIR / "_china_ne_outline.geojson",
            REPO / "out" / "data" / "cache" / "china_city_src" / "china_outline.geojson",
            CACHE / "china_outline.geojson",
        ]
        cutline = CACHE / "china_outline_cut.geojson"
        if ensure_cutline(outline_candidates, cutline):
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
            print(
                "WARN: no NE land outline; run build_china_city.py first "
                "or pass --no-cutline. Skipping cutline.",
                flush=True,
            )

    out.parent.mkdir(parents=True, exist_ok=True)
    run_gdal(bin_dir, "gdal_translate", [
        "-ot", "Float32",
        "-co", "COMPRESS=LZW",
        "-co", "TILED=YES",
        str(final_src),
        str(out),
    ])
    run_gdal(bin_dir, "gdalinfo", ["-stats", str(out)])
    # Refresh the GN copy destination (out/data/) beside testing/data/.
    out_data = REPO / "out" / "data" / out.name
    if out.resolve() != out_data.resolve():
        out_data.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(out, out_data)
        print(f"copied → {out_data}", flush=True)
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
    outline = Path(__file__).resolve().parent / "_china_ne_outline.geojson"
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
    parser.add_argument(
        "--zoom",
        type=int,
        default=7,
        help="AWS terrain tile zoom (6=coarse, 7=default national, 8=heavier)",
    )
    # ~0.040° / cell (~4.5 km) — 2× prior 720×450 so map2d hillshade bake
    # (max_edge≈1024) still samples real DEM slopes, not soft bilinear mush.
    parser.add_argument("--cols", type=int, default=1536)
    parser.add_argument("--rows", type=int, default=960)
    parser.add_argument("--no-cutline", action="store_true",
                        help="Keep full bbox without national outline mask")
    parser.add_argument(
        "--jobs",
        type=int,
        default=DEFAULT_JOBS,
        help=f"parallel tile download workers (default: {DEFAULT_JOBS})",
    )
    args = parser.parse_args()
    if args.source == "synthetic":
        return build_synthetic(args.out, args.cols, args.rows)
    try:
        return build_real(
            args.out,
            args.zoom,
            args.cols,
            args.rows,
            apply_cutline=not args.no_cutline,
            jobs=args.jobs,
        )
    except Exception as e:  # noqa: BLE001
        print(f"real DEM failed ({e}); falling back to synthetic", flush=True)
        return build_synthetic(args.out, args.cols, args.rows)


if __name__ == "__main__":
    sys.exit(main())
