#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build testing/data/pointcloud_public_sample.txt from public china_dem.tif.

Source: china_dem.tif (Mapzen/Nextzen terrain stack — SRTM/GMTED/NED/ETOPO
derivatives; see china_city.LICENSE.txt). Not a survey product.

Output format matches leftover Smt3DPointCloud::Read3DPointCloud:
  x,z,y,r,g,b   (comma; second column → leftover Z, third → Y; RGB 0–255)

This third_party GDAL build often omits the XYZ writer. Prefer XYZ when
available; otherwise subsample with gdallocationinfo -geoloc on a grid.

Usage:
  py -3 testing/data/build_pointcloud_sample.py
  py -3 testing/data/build_pointcloud_sample.py --stride 32 --max-points 8000
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_DEM = Path(__file__).resolve().parent / "china_dem.tif"
DEFAULT_OUT = Path(__file__).resolve().parent / "pointcloud_public_sample.txt"


def find_gdal_bin() -> Path:
    for d in (
        REPO / "third_party" / ".install" / "bin",
        REPO / "out" / "third_party" / "bin",
    ):
        if (d / "gdal_translate.exe").is_file() or (d / "gdal_translate").is_file():
            return d
    raise FileNotFoundError("gdal_translate not found under third_party/.install/bin")


def gdal_exe(bin_dir: Path, name: str) -> str:
    win = bin_dir / f"{name}.exe"
    if win.is_file():
        return str(win)
    unix = bin_dir / name
    if unix.is_file():
        return str(unix)
    raise FileNotFoundError(f"{name} not in {bin_dir}")


def hypsometric_rgb(z_m: float) -> tuple[int, int, int]:
    """Simple elevation wash (matches leftover DEM carto intent)."""
    if z_m < 50:
        return (40, 120, 60)
    if z_m < 500:
        return (90, 150, 70)
    if z_m < 1500:
        return (160, 140, 80)
    if z_m < 3500:
        return (140, 110, 70)
    return (230, 230, 235)


def _run(cmd: list[str], env: dict[str, str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )


def xyz_driver_available(bin_dir: Path, env: dict[str, str]) -> bool:
    info = _run([gdal_exe(bin_dir, "gdalinfo"), "--formats"], env)
    return bool(re.search(r"(?im)^\s*XYZ\s*-\s*raster", info.stdout or ""))


def dem_to_xyz_lines(dem: Path, bin_dir: Path, env: dict[str, str]) -> list[tuple[float, float, float]]:
    with tempfile.TemporaryDirectory(prefix="pc_xyz_") as tmp:
        xyz = Path(tmp) / "dem.xyz"
        cmd = [
            gdal_exe(bin_dir, "gdal_translate"),
            "-of",
            "XYZ",
            str(dem),
            str(xyz),
        ]
        print("+", " ".join(cmd), flush=True)
        completed = _run(cmd, env)
        if completed.returncode != 0:
            raise RuntimeError(
                (completed.stderr or completed.stdout or "gdal_translate XYZ failed").strip()
            )
        pts: list[tuple[float, float, float]] = []
        with xyz.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.replace(",", " ").split()
                if len(parts) < 3:
                    continue
                try:
                    lon = float(parts[0])
                    lat = float(parts[1])
                    z = float(parts[2])
                except ValueError:
                    continue
                if z != z:  # NaN
                    continue
                if z < -500:
                    continue
                pts.append((lon, lat, z))
        return pts


def parse_gdalinfo_grid(dem: Path, bin_dir: Path, env: dict[str, str]) -> tuple[int, int, float, float, float, float]:
    """Return cols, rows, origin_x, origin_y, pixel_w, pixel_h from gdalinfo."""
    info = _run([gdal_exe(bin_dir, "gdalinfo"), str(dem)], env)
    if info.returncode != 0:
        raise RuntimeError((info.stderr or info.stdout or "gdalinfo failed").strip())
    text = info.stdout or ""
    size_m = re.search(r"Size is\s+(\d+),\s*(\d+)", text)
    origin_m = re.search(r"Origin\s*=\s*\(([-\d.]+),\s*([-\d.]+)\)", text)
    pix_m = re.search(r"Pixel Size\s*=\s*\(([-\d.]+),\s*([-\d.]+)\)", text)
    if not (size_m and origin_m and pix_m):
        raise RuntimeError("gdalinfo missing Size/Origin/Pixel Size")
    return (
        int(size_m.group(1)),
        int(size_m.group(2)),
        float(origin_m.group(1)),
        float(origin_m.group(2)),
        float(pix_m.group(1)),
        float(pix_m.group(2)),
    )


def dem_to_grid_lines(
    dem: Path,
    bin_dir: Path,
    env: dict[str, str],
    *,
    stride: int,
    max_points: int,
) -> list[tuple[float, float, float]]:
    """Subsample DEM with gdallocationinfo (no XYZ writer required)."""
    cols, rows, ox, oy, pw, ph = parse_gdalinfo_grid(dem, bin_dir, env)
    step = max(1, stride)
    loc = gdal_exe(bin_dir, "gdallocationinfo")
    pts: list[tuple[float, float, float]] = []
    for row in range(0, rows, step):
        for col in range(0, cols, step):
            lon = ox + (col + 0.5) * pw
            lat = oy + (row + 0.5) * ph
            cmd = [loc, "-valonly", "-geoloc", str(dem), f"{lon}", f"{lat}"]
            completed = _run(cmd, env)
            if completed.returncode != 0:
                continue
            raw = (completed.stdout or "").strip().splitlines()
            if not raw:
                continue
            try:
                z = float(raw[-1].strip())
            except ValueError:
                continue
            if z != z or z < -500:
                continue
            pts.append((lon, lat, z))
            if len(pts) >= max_points:
                return pts
    return pts


def write_pointcloud(
    pts: list[tuple[float, float, float]],
    out: Path,
    *,
    stride: int,
    max_points: int,
) -> int:
    # Leftover Read3DPointCloud: sscanf "%f,%f,%f,%d,%d,%d" → x,z,y,r,g,b
    # then multiplies position by 10. Use lon → x, elev → z-col, lat → y-col.
    sampled = pts[:: max(1, stride)] if stride > 1 and len(pts) > max_points else pts
    if len(sampled) > max_points:
        step = max(1, len(sampled) // max_points)
        sampled = sampled[::step][:max_points]
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="ascii", newline="\n") as f:
        f.write(
            "# Public china_dem subsample for Smt3DPointCloud::Read3DPointCloud\n"
        )
        f.write("# Format: x,z,y,r,g,b  (lon, elev_m, lat, RGB)\n")
        for lon, lat, z in sampled:
            r, g, b = hypsometric_rgb(z)
            f.write(f"{lon:.5f},{z:.2f},{lat:.5f},{r},{g},{b}\n")
    return len(sampled)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dem", type=Path, default=DEFAULT_DEM)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--stride", type=int, default=48, help="Keep every Nth DEM cell")
    ap.add_argument("--max-points", type=int, default=6000)
    args = ap.parse_args()
    if not args.dem.is_file():
        print(f"missing DEM: {args.dem}", file=sys.stderr)
        print("Run: py -3 testing/data/build_china_dem.py", file=sys.stderr)
        return 2
    bin_dir = find_gdal_bin()
    env = os.environ.copy()
    env["PATH"] = str(bin_dir) + os.pathsep + env.get("PATH", "")

    pts: list[tuple[float, float, float]]
    if xyz_driver_available(bin_dir, env):
        pts = dem_to_xyz_lines(args.dem, bin_dir, env)
        n = write_pointcloud(
            pts, args.out, stride=args.stride, max_points=args.max_points
        )
    else:
        print("XYZ writer unavailable — sampling via gdallocationinfo", flush=True)
        # Grid already applies stride; do not thin again in write_pointcloud.
        pts = dem_to_grid_lines(
            args.dem,
            bin_dir,
            env,
            stride=args.stride,
            max_points=args.max_points,
        )
        n = write_pointcloud(pts, args.out, stride=1, max_points=args.max_points)

    if n < 100:
        print(f"too few DEM samples: {n}", file=sys.stderr)
        return 3
    print(f"wrote {n} points → {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
