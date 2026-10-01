#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build formal traffic/flood plugin fixtures under testing/data/plugin/.

Traffic network GeoJSON is authored in-tree. Flood basin DEM is composed with
GDAL CLI (gdal_create + gdal_rasterize) from flood_valley.geojson.

Usage (repo root):
  py -3 testing/data/build_plugin_traffic_flood_samples.py
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


def build_flood_dem(out_tif: Path) -> None:
  valley = PLUGIN / "flood_valley.geojson"
  if not valley.is_file():
    raise FileNotFoundError(valley)
  out_tif.parent.mkdir(parents=True, exist_ok=True)
  if out_tif.exists():
    out_tif.unlink()

  # Wuhan-area schematic window; hillside ~120m, basin/channel lower burns.
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

  # Burn nested valley / channel / thalweg (order: high → low).
  for burn in ("48", "28", "18"):
    # Filter by elev_burn via SQL where supported; else burn all then override.
    pass

  # Three passes: basin 48, channel 28, thalweg 18 (later overwrites).
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
      "--skip-dem",
      action="store_true",
      help="Only verify traffic geojson is present",
  )
  args = ap.parse_args()

  traffic = PLUGIN / "traffic_network_sample.geojson"
  if not traffic.is_file():
    print(f"missing {traffic}", file=sys.stderr)
    return 1
  print(f"traffic ok: {traffic} ({traffic.stat().st_size} bytes)")

  if args.skip_dem:
    return 0

  dem = PLUGIN / "flood_basin_sample.tif"
  build_flood_dem(dem)
  print(f"flood dem ok: {dem} ({dem.stat().st_size} bytes)")

  # Keep valley geojson as source for rebuild; not required at runtime.
  readme = PLUGIN / "README.md"
  if readme.is_file():
    text = readme.read_text(encoding="utf-8")
    marker = "traffic_network_sample.geojson"
    if marker not in text:
      print("note: update plugin/README.md manually for new fixtures")
  return 0


if __name__ == "__main__":
  sys.exit(main())
