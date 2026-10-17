# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Yangtze and Yellow River mouths: land vertices and named river tips.

Reads out/data/china_city.gpkg via ogr2ogr. Each named river's eastern tip
must fall inside its mouth window, and must not sit more than --tol-deg
(default 1.0) west of that window's easternmost land vertex.

    py -3 testing/tools/harness/_shared/case/align/china_estuary.py
    py -3 testing/tools/harness/_shared/case/align/china_estuary.py --self-check
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def repo_root() -> Path:
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "build.bat").is_file() and (parent / "testing").is_dir():
            return parent
    raise RuntimeError("repo root not found")


# (xmin, ymin, xmax, ymax), river name. Windows are the two estuary corridors.
MOUTHS = (
    ("yangtze", (121.0, 30.8, 122.5, 31.8), "长江"),
    ("yellow", (118.5, 37.3, 120.0, 38.2), "黄河"),
)


def walk_coords(geom, out: list) -> None:
    coords = geom.get("coordinates")
    if not coords:
        return

    def rec(node) -> None:
        if isinstance(node[0], (int, float)):
            out.append(node)
            return
        for child in node:
            rec(child)

    rec(coords)


def eastern_lon(coords: list) -> float:
    return max(coords, key=lambda p: p[0])[0]


def tip_in_window(tip_lon: float, xmin: float, xmax: float, pad_deg: float = 0.05) -> bool:
    """True when the river's eastern tip falls inside the mouth corridor."""
    return (xmin - pad_deg) <= tip_lon <= (xmax + pad_deg)


def east_gap_ok(land_max_lon: float, river_tip_lon: float, tol_deg: float) -> bool:
    """True when the centerline is not more than tol west of the land spit.

    The Yellow sample gap is about 0.76 deg (channel ends before the eastern
    land vertex). 1.0 deg fails a river that never reaches the outer mouth.
    """
    return (land_max_lon - river_tip_lon) <= tol_deg


def _ogr2ogr(root: Path) -> Path:
    return root / "out" / "third_party" / "bin" / "ogr2ogr.exe"


def _ogr(ogr: Path, dest: Path, args: list[str]) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    # GeoJSON has no DeleteLayer; -overwrite fails once the file exists.
    if dest.exists():
        dest.unlink()
    proc = subprocess.run(
        [str(ogr), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or f"ogr2ogr rc={proc.returncode}")


def _export(ogr: Path, gpkg: Path, dest: Path, *, where: str, layer: str) -> None:
    _ogr(
        ogr,
        dest,
        [
            "-f",
            "GeoJSON",
            "-where",
            where,
            str(dest),
            str(gpkg),
            layer,
        ],
    )


def _features(path: Path) -> list:
    data = json.loads(path.read_text(encoding="utf-8"))
    return list(data.get("features") or [])


def check_mouths(root: Path, *, tol_deg: float) -> dict:
    ogr = _ogr2ogr(root)
    gpkg = root / "out" / "data" / "china_city.gpkg"
    if not ogr.is_file():
        raise FileNotFoundError(ogr)
    if not gpkg.is_file():
        raise FileNotFoundError(gpkg)
    out_dir = root / "out" / "Debug" / "captures" / "analysis" / "estuary"
    out_dir.mkdir(parents=True, exist_ok=True)
    report: dict = {"tol_deg": tol_deg, "mouths": []}
    ok = True
    for name, (xmin, ymin, xmax, ymax), river in MOUTHS:
        land_path = out_dir / f"{name}_land.geojson"
        _ogr(
            ogr,
            land_path,
            [
                "-f",
                "GeoJSON",
                "-spat",
                str(xmin),
                str(ymin),
                str(xmax),
                str(ymax),
                str(land_path),
                str(gpkg),
                "area",
            ],
        )
        land = []
        for feat in _features(land_path):
            walk_coords(feat.get("geometry") or {}, land)
        land_in = [p for p in land if xmin <= p[0] <= xmax and ymin <= p[1] <= ymax]
        river_path = out_dir / f"{name}_river.geojson"
        river_where = f"name = '{river}'"
        _export(ogr, gpkg, river_path, where=river_where, layer="line")
        tips = []
        for feat in _features(river_path):
            coords = []
            walk_coords(feat.get("geometry") or {}, coords)
            if coords:
                tips.append(eastern_lon(coords))
        land_max = eastern_lon(land_in) if land_in else None
        tip = max(tips) if tips else None
        in_window = tip is not None and tip_in_window(tip, xmin, xmax)
        gap_ok = (
            land_max is not None and tip is not None and east_gap_ok(land_max, tip, tol_deg)
        )
        aligned = bool(land_in) and in_window and gap_ok
        ok = ok and aligned
        report["mouths"].append(
            {
                "name": name,
                "river": river,
                "land_vertices": len(land_in),
                "land_max_lon": land_max,
                "river_tip_lon": tip,
                "in_window": in_window,
                "east_gap_deg": None
                if land_max is None or tip is None
                else round(land_max - tip, 4),
                "aligned": aligned,
            }
        )
    report["ok"] = ok
    (out_dir / "estuary_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return report


def self_check() -> int:
    if not tip_in_window(121.56, 121.0, 122.5):
        print("self-check: yangtze tip should sit in the window", file=sys.stderr)
        return 1
    if tip_in_window(118.0, 121.0, 122.5):
        print("self-check: inland tip should miss the window", file=sys.stderr)
        return 1
    if not east_gap_ok(119.998, 119.235, 1.0):
        print("self-check: yellow sample gap should pass 1.0 deg", file=sys.stderr)
        return 1
    if east_gap_ok(122.0, 120.0, 1.0):
        print("self-check: 2 deg shortfall should fail", file=sys.stderr)
        return 1
    print("self-check ok")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-check", action="store_true")
    parser.add_argument("--tol-deg", type=float, default=1.0)
    args = parser.parse_args(argv)
    if args.self_check:
        return self_check()
    try:
        report = check_mouths(repo_root(), tol_deg=args.tol_deg)
    except FileNotFoundError as exc:
        print(f"missing {exc}", file=sys.stderr)
        return 2
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 2
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
