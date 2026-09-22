#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build offline china_city.gpkg (area / line / point / text) for Views.

Downloads Aliyun DataV prefecture polygons, Natural Earth rivers, and Natural
Earth trunk roads (cached under out/china_city_src/), then writes EPSG:4326
layers:

  area   MultiPolygon  name, adcode
  line   MultiLineString name, kind[, class]
         kind=river|lake|road; class=motorway|trunk|primary for roads
  point  Point         name, kind=city
  text   Point         anno, name, angle, color

Usage:
  py -3 testing/data/build_china_city.py
  py -3 testing/data/build_china_city.py --out out/china_city.gpkg

Requires: Python 3.10+, pyshp (for NE shapefiles). Network on first run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sqlite3
import struct
import sys
import time
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CACHE = REPO / "out" / "china_city_src"
DEFAULT_OUT = Path(__file__).resolve().parent / "china_city.gpkg"

DATAV_CHINA = "https://geo.datav.aliyun.com/areas_v3/bound/100000_full.json"
DATAV_PROV = "https://geo.datav.aliyun.com/areas_v3/bound/{code}_full.json"
DATAV_PLAIN = "https://geo.datav.aliyun.com/areas_v3/bound/{code}.json"
NE_RIVERS_URLS = (
    "https://naciscdn.org/naturalearth/10m/physical/ne_10m_rivers_lake_centerlines.zip",
    "https://naturalearth.s3.amazonaws.com/10m_physical/ne_10m_rivers_lake_centerlines.zip",
)
NE_ROADS_URLS = (
    "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_roads.zip",
    "https://naturalearth.s3.amazonaws.com/10m_cultural/ne_10m_roads.zip",
)

# Direct-controlled municipalities / SARs / Taiwan: keep province polygon as
# one prefecture-level area instead of district fragments.
KEEP_PROVINCE_AS_AREA = {
    110000,
    120000,
    310000,
    500000,
    710000,
    810000,
    820000,
}

# Approximate China bbox for river clip (degrees, CRS84). Matches
# app::kChinaLonLatExtent so leftover / Views overview framing agrees.
CHINA_BBOX = (73.0, 18.0, 135.0, 54.0)


def _download(url: str, dest: Path, timeout: int = 180) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists() and dest.stat().st_size > 1000:
        return
    print(f"GET {url}", flush=True)
    urllib.request.urlretrieve(url, dest)
    print(f"  -> {dest} ({dest.stat().st_size} bytes)", flush=True)


def _download_first(urls: tuple[str, ...], dest: Path) -> None:
    if dest.exists() and dest.stat().st_size > 1000:
        return
    last: Exception | None = None
    for url in urls:
        try:
            _download(url, dest)
            return
        except Exception as e:  # noqa: BLE001 — try mirrors
            last = e
            print(f"  fail {url}: {e}", flush=True)
    raise RuntimeError(f"download failed for {dest}: {last}")


def fetch_sources(cache: Path) -> tuple[Path, Path, Path, Path]:
    cache.mkdir(parents=True, exist_ok=True)
    china = cache / "china_full.json"
    _download(DATAV_CHINA, china)

    provinces = cache / "provinces"
    provinces.mkdir(exist_ok=True)
    china_fc = json.loads(china.read_text(encoding="utf-8"))
    for feat in china_fc.get("features") or []:
        props = feat.get("properties") or {}
        code = props.get("adcode")
        if code is None:
            continue
        code_s = str(code)
        if not code_s.isdigit():
            continue
        code_i = int(code_s)
        dest = provinces / f"{code_i}_full.json"
        if dest.exists() and dest.stat().st_size > 1000:
            continue
        try:
            _download(DATAV_PROV.format(code=code_i), dest)
        except Exception:
            _download(DATAV_PLAIN.format(code=code_i), dest)
        time.sleep(0.08)

    rivers_zip = cache / "ne_rivers.zip"
    _download_first(NE_RIVERS_URLS, rivers_zip)
    rivers_dir = cache / "ne_rivers"
    rivers_marker = rivers_dir / "ne_10m_rivers_lake_centerlines.shp"
    if not rivers_marker.exists():
        rivers_dir.mkdir(exist_ok=True)
        with zipfile.ZipFile(rivers_zip, "r") as zf:
            zf.extractall(rivers_dir)

    roads_zip = cache / "ne_roads.zip"
    _download_first(NE_ROADS_URLS, roads_zip)
    roads_dir = cache / "ne_roads"
    roads_marker = roads_dir / "ne_10m_roads.shp"
    if not roads_marker.exists():
        roads_dir.mkdir(exist_ok=True)
        with zipfile.ZipFile(roads_zip, "r") as zf:
            zf.extractall(roads_dir)

    return china, provinces, rivers_marker, roads_marker


# --- minimal WKB + GeoPackage writers (EPSG:4326) ---

WKB_POINT = 1
WKB_LINESTRING = 2
WKB_POLYGON = 3
WKB_MULTILINESTRING = 5
WKB_MULTIPOLYGON = 6


def _wkb_header(geom_type: int) -> bytes:
    return struct.pack("<BI", 1, geom_type)  # little-endian


def _pack_xy(x: float, y: float) -> bytes:
    return struct.pack("<dd", float(x), float(y))


def _ring_wkb(ring: list) -> bytes:
    parts = [struct.pack("<I", len(ring))]
    for pt in ring:
        parts.append(_pack_xy(pt[0], pt[1]))
    return b"".join(parts)


def geojson_to_wkb(geom: dict) -> bytes:
    """Convert GeoJSON geometry dict to ISO WKB (2D)."""
    gtype = geom.get("type")
    coords = geom.get("coordinates")
    if gtype == "Point":
        return _wkb_header(WKB_POINT) + _pack_xy(coords[0], coords[1])
    if gtype == "LineString":
        return _wkb_header(WKB_LINESTRING) + _ring_wkb(coords)
    if gtype == "MultiLineString":
        parts = [_wkb_header(WKB_MULTILINESTRING), struct.pack("<I", len(coords))]
        for line in coords:
            parts.append(_wkb_header(WKB_LINESTRING) + _ring_wkb(line))
        return b"".join(parts)
    if gtype == "Polygon":
        parts = [_wkb_header(WKB_POLYGON), struct.pack("<I", len(coords))]
        for ring in coords:
            parts.append(_ring_wkb(ring))
        return b"".join(parts)
    if gtype == "MultiPolygon":
        parts = [_wkb_header(WKB_MULTIPOLYGON), struct.pack("<I", len(coords))]
        for poly in coords:
            parts.append(_wkb_header(WKB_POLYGON))
            parts.append(struct.pack("<I", len(poly)))
            for ring in poly:
                parts.append(_ring_wkb(ring))
        return b"".join(parts)
    raise ValueError(f"unsupported geometry type: {gtype}")


def _envelope(geom: dict) -> tuple[float, float, float, float]:
    xs: list[float] = []
    ys: list[float] = []

    def walk(node: object) -> None:
        if isinstance(node, (list, tuple)):
            if node and isinstance(node[0], (int, float)):
                xs.append(float(node[0]))
                ys.append(float(node[1]))
            else:
                for child in node:
                    walk(child)

    walk(geom.get("coordinates"))
    if not xs:
        return (0.0, 0.0, 0.0, 0.0)
    return (min(xs), min(ys), max(xs), max(ys))


def gpkg_geom_blob(geom: dict, srs_id: int = 4326) -> bytes:
    """GeoPackageBinary empty envelope flags + WKB (flags bit1-3 = 0)."""
    wkb = geojson_to_wkb(geom)
    # magic GP, version 0, flags=0 (no envelope, little-endian WKB indicated
    # by WKB itself), srs_id
    return b"GP" + struct.pack("<BBI", 0, 0x01, srs_id) + wkb


def _extent_of_features(features: list[dict]) -> tuple[float, float, float, float]:
    minx = miny = math.inf
    maxx = maxy = -math.inf
    for f in features:
        a, b, c, d = _envelope(f["geometry"])
        minx, miny = min(minx, a), min(miny, b)
        maxx, maxy = max(maxx, c), max(maxy, d)
    if minx is math.inf:
        return (73.0, 18.0, 135.0, 54.0)
    return (minx, miny, maxx, maxy)


def write_gpkg(path: Path, layers: dict[str, dict]) -> None:
    """Write a minimal GeoPackage with named feature tables.

    layers: name -> {geom_type, columns: [(name, sql_type)], features: [{geom, props}]}
    """
    if path.exists():
        path.unlink()
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(path))
    cur = conn.cursor()
    cur.executescript(
        """
        PRAGMA application_id = 1196444487;
        PRAGMA user_version = 10200;
        CREATE TABLE gpkg_spatial_ref_sys (
          srs_name TEXT NOT NULL,
          srs_id INTEGER NOT NULL PRIMARY KEY,
          organization TEXT NOT NULL,
          organization_coordsys_id INTEGER NOT NULL,
          definition TEXT NOT NULL,
          description TEXT
        );
        CREATE TABLE gpkg_contents (
          table_name TEXT NOT NULL PRIMARY KEY,
          data_type TEXT NOT NULL,
          identifier TEXT UNIQUE,
          description TEXT DEFAULT '',
          last_change DATETIME NOT NULL DEFAULT
            (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
          min_x DOUBLE, min_y DOUBLE, max_x DOUBLE, max_y DOUBLE,
          srs_id INTEGER NOT NULL
        );
        CREATE TABLE gpkg_geometry_columns (
          table_name TEXT NOT NULL,
          column_name TEXT NOT NULL,
          geometry_type_name TEXT NOT NULL,
          srs_id INTEGER NOT NULL,
          z TINYINT NOT NULL,
          m TINYINT NOT NULL,
          CONSTRAINT pk_geom_cols PRIMARY KEY (table_name, column_name),
          CONSTRAINT fk_gc_tn FOREIGN KEY (table_name)
            REFERENCES gpkg_contents(table_name)
        );
        """
    )
    wgs84 = (
        'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,'
        '298.257223563]],PRIMEM["Greenwich",0],'
        'UNIT["degree",0.0174532925199433]]'
    )
    cur.executemany(
        "INSERT INTO gpkg_spatial_ref_sys VALUES (?,?,?,?,?,?)",
        [
            ("Undefined Cartesian", -1, "NONE", -1, "undefined", None),
            ("Undefined Geographic", 0, "NONE", 0, "undefined", None),
            ("WGS 84", 4326, "EPSG", 4326, wgs84, "longitude/latitude"),
        ],
    )

    for table, spec in layers.items():
        geom_type = spec["geom_type"]
        columns = spec["columns"]  # list of (name, sql_type)
        features = spec["features"]
        col_sql = ", ".join(f'"{c}" {t}' for c, t in columns)
        cur.execute(
            f'CREATE TABLE "{table}" ('
            f"fid INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "
            f"geom BLOB NOT NULL"
            f'{(", " + col_sql) if col_sql else ""})'
        )
        minx, miny, maxx, maxy = _extent_of_features(features)
        cur.execute(
            "INSERT INTO gpkg_contents "
            "(table_name, data_type, identifier, description, last_change, "
            "min_x, min_y, max_x, max_y, srs_id) "
            "VALUES (?, 'features', ?, ?, '2026-09-18T00:00:00.000Z', "
            "?, ?, ?, ?, 4326)",
            (table, table, table, minx, miny, maxx, maxy),
        )
        cur.execute(
            "INSERT INTO gpkg_geometry_columns "
            "(table_name, column_name, geometry_type_name, srs_id, z, m) "
            "VALUES (?, 'geom', ?, 4326, 0, 0)",
            (table, geom_type),
        )
        col_names = [c for c, _ in columns]
        placeholders = ", ".join(["?"] * (1 + len(col_names)))
        insert_sql = (
            f'INSERT INTO "{table}" (geom'
            + ((", " + ", ".join(f'"{c}"' for c in col_names)) if col_names else "")
            + f") VALUES ({placeholders})"
        )
        for feat in features:
            blob = gpkg_geom_blob(feat["geometry"])
            props = feat.get("properties") or {}
            values = [blob] + [props.get(c) for c in col_names]
            cur.execute(insert_sql, values)

    conn.commit()
    conn.close()


def _centroid_of(geom: dict) -> tuple[float, float]:
    """Lightweight representative point (average of exterior ring vertices)."""
    gtype = geom.get("type")
    coords = geom.get("coordinates")
    ring = None
    if gtype == "Polygon":
        ring = coords[0]
    elif gtype == "MultiPolygon":
        # largest ring by vertex count
        ring = max((poly[0] for poly in coords), key=len)
    if not ring:
        return (0.0, 0.0)
    xs = [p[0] for p in ring[:-1] or ring]
    ys = [p[1] for p in ring[:-1] or ring]
    return (sum(xs) / len(xs), sum(ys) / len(ys))


def build_area_features(china_path: Path, provinces_dir: Path) -> list[dict]:
    china_fc = json.loads(china_path.read_text(encoding="utf-8"))
    by_code: dict[int, dict] = {}
    for feat in china_fc.get("features") or []:
        props = feat.get("properties") or {}
        code = props.get("adcode")
        if code is None or not str(code).isdigit():
            continue
        by_code[int(code)] = feat

    areas: list[dict] = []
    seen: set[int] = set()

    for code in sorted(KEEP_PROVINCE_AS_AREA):
        feat = by_code.get(code)
        if not feat:
            # Taiwan / SAR may only exist as plain download under provinces/
            plain = provinces_dir / f"{code}_full.json"
            if plain.exists():
                fc = json.loads(plain.read_text(encoding="utf-8"))
                feats = fc.get("features") or []
                if len(feats) == 1:
                    feat = feats[0]
                elif feats:
                    # dissolve not available — keep first / or province from china
                    feat = feats[0]
        if not feat:
            continue
        props = feat.get("properties") or {}
        name = props.get("name") or str(code)
        areas.append(
            {
                "geometry": feat["geometry"],
                "properties": {"name": name, "adcode": str(code)},
            }
        )
        seen.add(code)

    for path in sorted(provinces_dir.glob("*_full.json")):
        code_i = int(path.stem.split("_")[0])
        if code_i in KEEP_PROVINCE_AS_AREA:
            continue
        fc = json.loads(path.read_text(encoding="utf-8"))
        for feat in fc.get("features") or []:
            props = feat.get("properties") or {}
            level = props.get("level")
            adcode = props.get("adcode")
            if level != "city":
                continue
            if adcode is None:
                continue
            ad_i = int(adcode) if str(adcode).isdigit() else None
            if ad_i is None or ad_i in seen:
                continue
            name = props.get("name") or str(adcode)
            # Prefer DataV centroid when present (more stable than vertex mean).
            areas.append(
                {
                    "geometry": feat["geometry"],
                    "properties": {
                        "name": name,
                        "adcode": str(adcode),
                        "_center": props.get("centroid") or props.get("center"),
                    },
                }
            )
            seen.add(ad_i)

    return areas


def build_point_text(areas: list[dict]) -> tuple[list[dict], list[dict]]:
    points: list[dict] = []
    texts: list[dict] = []
    for area in areas:
        props = area["properties"]
        name = props["name"]
        center = props.pop("_center", None)
        if isinstance(center, (list, tuple)) and len(center) >= 2:
            x, y = float(center[0]), float(center[1])
        else:
            x, y = _centroid_of(area["geometry"])
        geom = {"type": "Point", "coordinates": [x, y]}
        points.append(
            {
                "geometry": geom,
                "properties": {"name": name, "kind": "city"},
            }
        )
        texts.append(
            {
                "geometry": {
                    "type": "Point",
                    "coordinates": [x, y + 0.05],  # slight offset for label
                },
                "properties": {
                    "anno": name,
                    "name": name,
                    "angle": 0.0,
                    "color": "#222222",
                },
            }
        )
    return points, texts


def _pt_in_bbox(x: float, y: float, bbox: tuple[float, float, float, float] = CHINA_BBOX) -> bool:
    return bbox[0] <= x <= bbox[2] and bbox[1] <= y <= bbox[3]


def _clip_segment_to_bbox(
    coords: list, bbox: tuple[float, float, float, float] = CHINA_BBOX
) -> list[list]:
    """Keep contiguous runs of vertices inside bbox; drop foreign stubs.

    Natural Earth rivers that only touch China otherwise keep Siberia /
    Central Asia tails and paint outside provincial land in the 2D overview.
    """
    runs: list[list] = []
    cur: list = []
    for pt in coords:
        x, y = float(pt[0]), float(pt[1])
        if _pt_in_bbox(x, y, bbox):
            cur.append([x, y])
        else:
            if len(cur) >= 2:
                runs.append(cur)
            cur = []
    if len(cur) >= 2:
        runs.append(cur)
    return runs


def build_line_features(shp_path: Path) -> list[dict]:
    try:
        import shapefile  # type: ignore
    except ImportError as e:
        raise SystemExit(
            "pyshp is required to read Natural Earth rivers. "
            "Install: py -3 -m pip install pyshp"
        ) from e

    sf = shapefile.Reader(str(shp_path))
    fields = [f[0] for f in sf.fields[1:]]
    lines: list[dict] = []
    for sr in sf.iterShapeRecords():
        shape = sr.shape
        rec = dict(zip(fields, sr.record))
        if shape.shapeType not in (3, 13, 23):  # polyline variants
            # shapefile may use POLYLINE = 3
            pass
        parts = list(shape.parts) + [len(shape.points)]
        segments: list[list] = []
        for i in range(len(parts) - 1):
            seg = [[float(p[0]), float(p[1])] for p in shape.points[parts[i] : parts[i + 1]]]
            for clipped in _clip_segment_to_bbox(seg):
                segments.append(clipped)
        if not segments:
            continue
        name = rec.get("name") or rec.get("NAME") or rec.get("name_en") or ""
        if isinstance(name, bytes):
            name = name.decode("utf-8", errors="replace")
        name = str(name).strip()
        feature_cla = str(rec.get("featurecla") or rec.get("FEATURECLA") or "")
        kind = "river"
        if "Lake" in feature_cla or "lake" in feature_cla.lower():
            kind = "lake"
        geom = (
            {"type": "LineString", "coordinates": segments[0]}
            if len(segments) == 1
            else {"type": "MultiLineString", "coordinates": segments}
        )
        lines.append(
            {
                "geometry": geom,
                "properties": {"name": name, "kind": kind, "class": ""},
            }
        )

    # Prefer named / major rivers: keep features that intersect China densely.
    # Cap to a few hundred for size while retaining recognizable trunk rivers.
    if len(lines) > 400:
        named = [f for f in lines if f["properties"]["name"]]
        unnamed = [f for f in lines if not f["properties"]["name"]]
        lines = named[:350] + unnamed[:50]
    return lines


def _ne_road_class(type_raw: str, scalerank: int) -> str | None:
    """Map Natural Earth road type → OSM-like class, or None to drop."""
    t = (type_raw or "").strip().lower()
    if "ferry" in t or "track" in t:
        return None
    if "major highway" in t or "beltway" in t:
        return "motorway"
    if "secondary highway" in t:
        return "trunk"
    # NE 10m China often tags arterials as Unknown; keep low scalerank only.
    if t in ("", "unknown"):
        if scalerank <= 5:
            return "trunk"
        if scalerank <= 6:
            return "primary"
        return None
    # Keep a thin primary set for national framing; drop local roads.
    if t == "road" and scalerank <= 3:
        return "primary"
    return None


def build_road_features(shp_path: Path) -> list[dict]:
    """Natural Earth 10m roads clipped to China; motorway/trunk/primary only."""
    try:
        import shapefile  # type: ignore
    except ImportError as e:
        raise SystemExit(
            "pyshp is required to read Natural Earth roads. "
            "Install: py -3 -m pip install pyshp"
        ) from e

    sf = shapefile.Reader(str(shp_path))
    fields = [f[0] for f in sf.fields[1:]]
    roads: list[dict] = []
    for sr in sf.iterShapeRecords():
        shape = sr.shape
        rec = dict(zip(fields, sr.record))
        type_raw = str(rec.get("type") or rec.get("TYPE") or "")
        try:
            scalerank = int(rec.get("scalerank") or rec.get("SCALERANK") or 99)
        except (TypeError, ValueError):
            scalerank = 99
        road_class = _ne_road_class(type_raw, scalerank)
        if not road_class:
            continue
        parts = list(shape.parts) + [len(shape.points)]
        segments: list[list] = []
        for i in range(len(parts) - 1):
            seg = [
                [float(p[0]), float(p[1])]
                for p in shape.points[parts[i] : parts[i + 1]]
            ]
            for clipped in _clip_segment_to_bbox(seg):
                segments.append(clipped)
        if not segments:
            continue
        name = rec.get("name") or rec.get("NAME") or rec.get("name_en") or ""
        if isinstance(name, bytes):
            name = name.decode("utf-8", errors="replace")
        name = str(name).strip()
        geom = (
            {"type": "LineString", "coordinates": segments[0]}
            if len(segments) == 1
            else {"type": "MultiLineString", "coordinates": segments}
        )
        roads.append(
            {
                "geometry": geom,
                "properties": {
                    "name": name,
                    "kind": "road",
                    "class": road_class,
                },
            }
        )

    # Cap size with a balanced motorway/trunk/primary mix for national framing.
    if len(roads) > 1500:
        motorway = [f for f in roads if f["properties"]["class"] == "motorway"]
        trunk = [f for f in roads if f["properties"]["class"] == "trunk"]
        primary = [f for f in roads if f["properties"]["class"] == "primary"]
        roads = motorway[:700] + trunk[:500] + primary[:300]
    return roads

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_OUT,
        help=f"output gpkg path (default: {DEFAULT_OUT})",
    )
    ap.add_argument(
        "--cache",
        type=Path,
        default=CACHE,
        help=f"download cache (default: {CACHE})",
    )
    ap.add_argument(
        "--also-out-dir",
        type=Path,
        default=REPO / "out",
        help="also copy finished gpkg here (default: repo out/)",
    )
    args = ap.parse_args()

    china, provinces, rivers_shp, roads_shp = fetch_sources(args.cache)
    print("Building area…", flush=True)
    areas = build_area_features(china, provinces)
    print(f"  area features: {len(areas)}", flush=True)
    points, texts = build_point_text(areas)
    print(f"  point/text: {len(points)}/{len(texts)}", flush=True)
    print("Building lines from Natural Earth rivers…", flush=True)
    lines = build_line_features(rivers_shp)
    print(f"  river/lake features: {len(lines)}", flush=True)
    print("Building lines from Natural Earth roads…", flush=True)
    roads = build_road_features(roads_shp)
    print(f"  road features: {len(roads)}", flush=True)
    lines = lines + roads
    print(f"  line features total: {len(lines)}", flush=True)

    layers = {
        "area": {
            "geom_type": "MULTIPOLYGON",
            "columns": [("name", "TEXT"), ("adcode", "TEXT")],
            "features": [
                {
                    "geometry": (
                        a["geometry"]
                        if a["geometry"]["type"] == "MultiPolygon"
                        else {
                            "type": "MultiPolygon",
                            "coordinates": [a["geometry"]["coordinates"]],
                        }
                    ),
                    "properties": {
                        "name": a["properties"]["name"],
                        "adcode": a["properties"]["adcode"],
                    },
                }
                for a in areas
            ],
        },
        "line": {
            "geom_type": "GEOMETRY",  # LineString or MultiLineString
            "columns": [("name", "TEXT"), ("kind", "TEXT"), ("class", "TEXT")],
            "features": lines,
        },
        "point": {
            "geom_type": "POINT",
            "columns": [("name", "TEXT"), ("kind", "TEXT")],
            "features": points,
        },
        "text": {
            "geom_type": "POINT",
            "columns": [
                ("anno", "TEXT"),
                ("name", "TEXT"),
                ("angle", "REAL"),
                ("color", "TEXT"),
            ],
            "features": texts,
        },
    }

    out: Path = args.out
    write_gpkg(out, layers)
    digest = sha256_file(out)
    size = out.stat().st_size
    print(f"Wrote {out} ({size} bytes)", flush=True)
    print(f"SHA256 {digest}", flush=True)

    # Sibling single-file GeoJSON (kind=) for hosts that still look for
    # china_city.geojson before .gpkg; same geometries as the GPKG layers.
    geojson_path = out.with_suffix(".geojson")
    fc_features: list[dict] = []
    for a in layers["area"]["features"]:
        fc_features.append(
            {
                "type": "Feature",
                "properties": {
                    "name": a["properties"]["name"],
                    "adcode": a["properties"]["adcode"],
                    "kind": "area",
                },
                "geometry": a["geometry"],
            }
        )
    for ln in lines:
        props = {
            "name": ln["properties"].get("name") or "",
            "kind": "line",
            "line_kind": ln["properties"].get("kind") or "river",
        }
        cls = ln["properties"].get("class") or ""
        if cls:
            props["class"] = cls
        fc_features.append(
            {
                "type": "Feature",
                "properties": props,
                "geometry": ln["geometry"],
            }
        )
    for pt in points:
        fc_features.append(
            {
                "type": "Feature",
                "properties": {"name": pt["properties"]["name"], "kind": "point"},
                "geometry": pt["geometry"],
            }
        )
    for tx in texts:
        fc_features.append(
            {
                "type": "Feature",
                "properties": {
                    "anno": tx["properties"]["anno"],
                    "name": tx["properties"]["name"],
                    "angle": tx["properties"]["angle"],
                    "color": tx["properties"]["color"],
                    "kind": "text",
                },
                "geometry": tx["geometry"],
            }
        )
    geojson_path.write_text(
        json.dumps(
            {
                "type": "FeatureCollection",
                "name": "china_city",
                "crs": {
                    "type": "name",
                    "properties": {"name": "urn:ogc:def:crs:OGC:1.3:CRS84"},
                },
                "features": fc_features,
            },
            ensure_ascii=False,
            separators=(",", ":"),
        ),
        encoding="utf-8",
    )
    print(f"Wrote {geojson_path} ({geojson_path.stat().st_size} bytes)", flush=True)

    pin_path = Path(__file__).resolve().parent / "china_city.PIN.txt"
    pin_path.write_text(
        f"file=china_city.gpkg\n"
        f"sha256={digest}\n"
        f"size={size}\n"
        f"geojson={geojson_path.name}\n"
        f"geojson_sha256={sha256_file(geojson_path)}\n"
        f"geojson_size={geojson_path.stat().st_size}\n",
        encoding="utf-8",
    )
    print(f"Wrote {pin_path}", flush=True)

    if args.also_out_dir:
        args.also_out_dir.mkdir(parents=True, exist_ok=True)
        for src in (out, geojson_path):
            dest = args.also_out_dir / src.name
            dest.write_bytes(src.read_bytes())
            print(f"Copied {dest}", flush=True)

    print(
        "counts:",
        f"area={len(areas)} line={len(lines)} "
        f"(roads={len(roads)}) point={len(points)} text={len(texts)}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
