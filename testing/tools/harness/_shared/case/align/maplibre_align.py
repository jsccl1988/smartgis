#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Dual stills: SmartGisViews StyleDocument vs MapLibre Native headless.

Shared Style: third_party/maplibre/example/style_align.json (copied to
out/*/maplibre/example/). Data: out/data/china_city.* �?same pack as the
main app (//testing/data:china_map_samples). Product never links mbgl.

    python testing/tools/harness/_shared/case/align/maplibre_align.py
    python testing/tools/harness/_shared/case/align/maplibre_align.py --skip-native

Outputs under out/Debug/maplibre/align/:
  product.bmp   �?SmartGisViews --map2d-showcase=align (china_city + style_align)
  native.png    �?maplibre_headless_example (when built)
  report.json   �?paths + sizes
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


def _repo_root() -> Path:
    p = Path(__file__).resolve().parent
    while p != p.parent:
        if (p / "build.bat").is_file() and (p / "testing").is_dir():
            return p
        p = p.parent
    raise RuntimeError("repo root not found")


ROOT = _repo_root()
OUT = ROOT / "out" / "Debug"
DATA = ROOT / "out" / "data"
ALIGN_DIR = OUT / "maplibre" / "align"
EXAMPLE_DIR = OUT / "maplibre" / "example"
EXAMPLE_SRC = ROOT / "third_party" / "maplibre" / "example"
EXE = OUT / "SmartGisViews.exe"
NATIVE = OUT / "maplibre" / "maplibre_headless_example.exe"
STYLE_NAME = "style_align.json"
PRODUCT_BMP = "map2d-showcase-align.bmp"
W = 640
H = 480
# Match map2d_showcase mainland framing (lon/lat).
BOUNDS_N, BOUNDS_W, BOUNDS_S, BOUNDS_E = 48.0, 80.0, 20.0, 128.0


def ensure_china_data() -> Path:
    geo = DATA / "china_city.geojson"
    if not geo.is_file():
        src = ROOT / "testing" / "data" / "china_city.geojson"
        if not src.is_file():
            raise FileNotFoundError(
                f"missing {geo} (and {src}); build //testing/data:china_map_samples"
            )
        DATA.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, geo)
        for name in (
            "china_city.gpkg",
            "china_city.style.json",
            "china_plp.geojson",
        ):
            s = ROOT / "testing" / "data" / name
            if s.is_file():
                shutil.copy2(s, DATA / name)
    return geo


def ensure_example_assets() -> Path:
    ensure_china_data()
    EXAMPLE_DIR.mkdir(parents=True, exist_ok=True)
    for name in (STYLE_NAME, "style_background.json"):
        src = EXAMPLE_SRC / name
        if src.is_file():
            shutil.copy2(src, EXAMPLE_DIR / name)
    # Native geojson source must be a same-dir file path (not ../.. URL).
    geo_src = DATA / "china_city.geojson"
    if not geo_src.is_file():
        geo_src = ROOT / "testing" / "data" / "china_city.geojson"
    if geo_src.is_file():
        shutil.copy2(geo_src, EXAMPLE_DIR / "china_city.geojson")
    style = EXAMPLE_DIR / STYLE_NAME
    if not style.is_file():
        raise FileNotFoundError(f"missing align style: {style}")
    if not (EXAMPLE_DIR / "china_city.geojson").is_file():
        raise FileNotFoundError(
            f"missing {EXAMPLE_DIR / 'china_city.geojson'}"
        )
    return style


def bmp_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"not a BMP: {path}")
    w, h = struct.unpack_from("<ii", data, 18)
    return abs(w), abs(h)


def png_size(path: Path) -> tuple[int, int] | None:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    if len(data) < 24:
        return None
    w, h = struct.unpack_from(">II", data, 16)
    return int(w), int(h)


def run_product() -> Path:
    if not EXE.is_file():
        raise FileNotFoundError(f"missing {EXE}; build SmartGisViews first")
    ensure_china_data()
    env = os.environ.copy()
    env["SMT_MAP2D_SHOWCASE_LINGER_MS"] = "0"
    cmd = [str(EXE), "--map2d-showcase=align"]
    print(" ".join(cmd), flush=True)
    completed = subprocess.run(cmd, cwd=str(OUT), env=env)
    if completed.returncode != 0:
        raise RuntimeError(f"product showcase exit {completed.returncode}")
    bmp = OUT / PRODUCT_BMP
    if not bmp.is_file():
        raise FileNotFoundError(f"missing product BMP: {bmp}")
    ALIGN_DIR.mkdir(parents=True, exist_ok=True)
    dest = ALIGN_DIR / "product.bmp"
    shutil.copy2(bmp, dest)
    return dest


def run_native(style: Path) -> Path | None:
    if not NATIVE.is_file():
        print(
            f"skip native: {NATIVE} not built (smt_enable_maplibre_example)",
            flush=True,
        )
        return None
    ensure_china_data()
    ALIGN_DIR.mkdir(parents=True, exist_ok=True)
    dest = ALIGN_DIR / "native.png"
    cmd = [
        str(NATIVE),
        "-s",
        str(style),
        "-o",
        str(dest),
        "-w",
        str(W),
        "-h",
        str(H),
        "-a",
        str(style.parent),
        "--bounds",
        str(BOUNDS_N),
        str(BOUNDS_W),
        str(BOUNDS_S),
        str(BOUNDS_E),
    ]
    print(" ".join(cmd), flush=True)
    completed = subprocess.run(cmd, cwd=str(style.parent))
    if completed.returncode != 0:
        raise RuntimeError(f"native headless exit {completed.returncode}")
    if not dest.is_file():
        raise FileNotFoundError(f"missing native PNG: {dest}")
    return dest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument("--skip-product", action="store_true")
    args = parser.parse_args()

    report: dict = {
        "style": STYLE_NAME,
        "data": "out/data/china_city.geojson (+ .gpkg for product)",
        "size": [W, H],
        "bounds_nwse": [BOUNDS_N, BOUNDS_W, BOUNDS_S, BOUNDS_E],
        "product": None,
        "native": None,
    }
    try:
        style = ensure_example_assets()
        report["style_path"] = str(style)
        report["china_geojson"] = str(DATA / "china_city.geojson")
        if not args.skip_product:
            product = run_product()
            pw, ph = bmp_size(product)
            report["product"] = {"path": str(product), "w": pw, "h": ph}
        if not args.skip_native:
            native = run_native(style)
            if native is not None:
                size = png_size(native)
                report["native"] = {
                    "path": str(native),
                    "w": size[0] if size else None,
                    "h": size[1] if size else None,
                }
    except Exception as exc:  # noqa: BLE001 �?CLI report surface
        report["error"] = str(exc)
        ALIGN_DIR.mkdir(parents=True, exist_ok=True)
        (ALIGN_DIR / "report.json").write_text(
            json.dumps(report, indent=2), encoding="utf-8"
        )
        print(f"maplibre_align FAIL: {exc}", file=sys.stderr)
        return 1

    ALIGN_DIR.mkdir(parents=True, exist_ok=True)
    (ALIGN_DIR / "report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8"
    )
    print(json.dumps(report, indent=2))
    print("maplibre_align PASS", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
