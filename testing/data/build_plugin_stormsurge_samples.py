#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build formal stormsurge plugin fixtures under testing/data/plugin/.

Creates a schematic coastal DEM (or copies flood basin DEM as fallback),
plus documents coast GeoJSON + tide CSV already authored in-tree.

Usage (repo root):
  py -3 testing/data/build_plugin_stormsurge_samples.py
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLUGIN = Path(__file__).resolve().parent / "plugin"
GDAL_BIN = ROOT / "third_party" / ".install" / "bin"


def gdal_exe(name: str) -> Path:
  p = GDAL_BIN / name
  if not p.is_file():
    raise FileNotFoundError(f"missing {p}")
  return p


def build_coastal_dem(out_tif: Path) -> None:
  """Schematic Wuhan-area coastal DEM (hillside + low basin near coast)."""
  out_tif.parent.mkdir(parents=True, exist_ok=True)
  if out_tif.exists():
    out_tif.unlink()

  create = [
      str(gdal_exe("gdal_create.exe")),
      "-of",
      "GTiff",
      "-ot",
      "Float32",
      "-outsize",
      "320",
      "240",
      "-bands",
      "1",
      "-burn",
      "120",
      "-a_srs",
      "EPSG:4326",
      "-a_ullr",
      "114.15",
      "30.65",
      "114.45",
      "30.45",
      "-a_nodata",
      "-9999",
      str(out_tif),
  ]
  subprocess.check_call(create, cwd=str(ROOT))

  valley = PLUGIN / "flood_valley.geojson"
  if valley.is_file():
    for burn, where in (
        ("48", "elev_burn = 48"),
        ("28", "elev_burn = 28"),
        ("18", "elev_burn = 18"),
    ):
      cmd = [
          str(gdal_exe("gdal_rasterize.exe")),
          "-burn",
          burn,
          "-where",
          where,
          str(valley),
          str(out_tif),
      ]
      subprocess.check_call(cmd, cwd=str(ROOT))

  info = subprocess.check_output(
      [str(gdal_exe("gdalinfo.exe")), "-mm", str(out_tif)],
      cwd=str(ROOT),
      text=True,
      errors="replace",
  )
  print(info)


def main() -> int:
  ap = argparse.ArgumentParser()
  ap.add_argument(
      "--copy-flood-dem",
      action="store_true",
      help="Copy flood_basin_sample.tif instead of regenerating DEM",
  )
  args = ap.parse_args()

  coast = PLUGIN / "stormsurge_coast_sample.geojson"
  tide = PLUGIN / "stormsurge_tide_sample.csv"
  if not coast.is_file() or not tide.is_file():
    print(f"missing coast/tide fixtures under {PLUGIN}", file=sys.stderr)
    return 1
  print(f"coast ok: {coast}")
  print(f"tide ok: {tide}")

  dem = PLUGIN / "stormsurge_dem_sample.tif"
  if args.copy_flood_dem:
    src = PLUGIN / "flood_basin_sample.tif"
    if not src.is_file():
      print(f"missing {src}", file=sys.stderr)
      return 1
    shutil.copy2(src, dem)
    print(f"dem copied: {dem}")
    return 0

  try:
    build_coastal_dem(dem)
  except (FileNotFoundError, subprocess.CalledProcessError) as exc:
    print(f"gdal dem build failed ({exc}); falling back to flood DEM copy",
          file=sys.stderr)
    src = PLUGIN / "flood_basin_sample.tif"
    if not src.is_file():
      return 1
    shutil.copy2(src, dem)
  print(f"stormsurge dem ok: {dem} ({dem.stat().st_size} bytes)")
  print(
      "Optional China coastal showcase: point --plugin-showcase=stormsurge "
      "at a real coastal DEM under out/data/ when available."
  )
  return 0


if __name__ == "__main__":
  sys.exit(main())
