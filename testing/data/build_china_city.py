#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Build offline china_city pack (vectors + optional DEM) for Views.

Vector themes are **China-only extracts**, all EPSG:4326 / OGC CRS84.
Do **not** mix Aliyun DataV (GCJ-02) with this pack.

  area / point / text — Natural Earth 10m cultural, ADM0 CHN/TWN/HKG/MAC
  line (river|lake|road) — vendored `china_hydro.src.geojson` and
    `china_roads.src.geojson` (China network, not the global NE river/road
    shapefiles). Build never downloads `ne_10m_rivers_lake_centerlines`.

Elevation (optional `--with-dem`): AWS Open Data / Mapzen terrain tiles,
warped to the same EPSG:4326 grid and cut with the NE admin land outline
so 2D vectors and 3D DEM share one land mask.

Layers (GPKG):

  area   MultiPolygon  name, adcode   (NE admin_1: CHN/TWN/HKG/MAC)
  line   MultiLineString name, kind[, class]
         kind=river|lake|road; class=motorway|trunk|primary for roads
  point  Point         name, kind=city  (NE populated places)
  text   Point         anno, name, angle, color

Also writes: china_outline.geojson (NE land cutline for DEM).

Usage:
  py -3 testing/data/build_china_city.py
  py -3 testing/data/build_china_city.py --with-dem
  py -3 testing/data/build_china_city.py --out out/china_city.gpkg

Requires: Python 3.10+, pyshp. Network on first run (cached under
out/data/cache/china_city_src/). DEM path needs GDAL CLI under
third_party/.install/bin.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sqlite3
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CACHE = REPO / "out" / "data" / "cache" / "china_city_src"
DEFAULT_OUT = Path(__file__).resolve().parent / "china_city.gpkg"

# Natural Earth 10m — same suite / same datum for every theme.
NE_ADMIN_URLS = (
    "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_admin_1_states_provinces.zip",
    "https://naturalearth.s3.amazonaws.com/10m_cultural/ne_10m_admin_1_states_provinces.zip",
)
NE_PLACES_URLS = (
    "https://naciscdn.org/naturalearth/10m/cultural/ne_10m_populated_places.zip",
    "https://naturalearth.s3.amazonaws.com/10m_cultural/ne_10m_populated_places.zip",
)

# China network extracts (committed). Not the global NE hydro/roads zips.
DATA_DIR = Path(__file__).resolve().parent
CHINA_HYDRO_SRC = DATA_DIR / "china_hydro.src.geojson"
CHINA_ROADS_SRC = DATA_DIR / "china_roads.src.geojson"

# Admin coverage matching the old demo pack (mainland + TW/HK/MO).
NE_ADMIN_ADM0 = frozenset({"CHN", "TWN", "HKG", "MAC"})

# Approximate China bbox for river/road clip (degrees, CRS84). Matches
# content::kChinaLonLatExtent so leftover / Views overview framing agrees.
CHINA_BBOX = (73.0, 18.0, 135.0, 54.0)

# NE 10m rivers have no stroke/adm0_a3. China hydrology is the vendored
# extract (china_hydro.src.geojson), not a runtime clip of the global zip.
YANGTZE_NAME_KEYS = frozenset(
    {"yangtze", "yangtzeriver", "changjiang", "jinsha", "jinshajiang", "长江", "金沙江"}
)
PEARL_NAME_KEYS = frozenset(
    {"pearl", "pearlriver", "xijiang", "珠江", "西江"}
)
HUANG_NAME_KEYS = frozenset(
    {"huang", "huanghe", "yellow", "yellowriver", "黄河"}
)

# Tier-1 / municipality seats that Natural Earth sampling has dropped from the
# china showcase pack (Beijing / Shanghai missing; mid-tier cities dominate).
# nameascii keys are matched case-insensitively; zh is the product label.
MUST_KEEP_CITIES: dict[str, tuple[str, float, float]] = {
    "beijing": ("北京", 116.4074, 39.9042),
    "peking": ("北京", 116.4074, 39.9042),
    "shanghai": ("上海", 121.4737, 31.2304),
    "guangzhou": ("广州", 113.2644, 23.1291),
    "canton": ("广州", 113.2644, 23.1291),
    "shenzhen": ("深圳", 114.0579, 22.5431),
    "chengdu": ("成都", 104.0665, 30.5723),
    "wuhan": ("武汉", 114.3055, 30.5928),
    "hangzhou": ("杭州", 120.1551, 30.2741),
    "chongqing": ("重庆", 106.5516, 29.5630),
    "tianjin": ("天津", 117.2008, 39.0842),
    "nanjing": ("南京", 118.7969, 32.0603),
    "xian": ("西安", 108.9398, 34.3416),
    "xi'an": ("西安", 108.9398, 34.3416),
    "zhengzhou": ("郑州", 113.6254, 34.7466),
}


def _city_zh_alias(name: str, name_ascii: str) -> str | None:
    """Map NE latin / mixed names onto stable Chinese product labels."""
    for key in (name_ascii, name):
        k = (key or "").strip().lower().replace(" ", "")
        if not k:
            continue
        if k in MUST_KEEP_CITIES:
            return MUST_KEEP_CITIES[k][0]
        # Strip common suffixes.
        for suf in ("shi", "city", "municipality"):
            if k.endswith(suf) and k[: -len(suf)] in MUST_KEEP_CITIES:
                return MUST_KEEP_CITIES[k[: -len(suf)]][0]
    return None


def _download(url: str, dest: Path, timeout: int = 180) -> None:
    import urllib.request

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


def _require_pyshp():
    try:
        import shapefile  # type: ignore
    except ImportError as e:
        raise SystemExit(
            "pyshp is required to read Natural Earth shapefiles. "
            "Install: py -3 -m pip install pyshp"
        ) from e
    return shapefile


def _unzip_shp(zip_path: Path, out_dir: Path, marker_name: str) -> Path:
    marker = out_dir / marker_name
    if marker.exists():
        return marker
    out_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "r") as zf:
        zf.extractall(out_dir)
    # NE zips sometimes nest one folder; search for the .shp.
    if not marker.exists():
        found = list(out_dir.rglob(marker_name))
        if not found:
            raise FileNotFoundError(f"{marker_name} missing after extract {zip_path}")
        return found[0]
    return marker


def fetch_sources(cache: Path) -> tuple[Path, Path]:
    """Download + extract NE 10m cultural themes (China admin + places only)."""
    cache.mkdir(parents=True, exist_ok=True)

    admin_zip = cache / "ne_admin1.zip"
    _download_first(NE_ADMIN_URLS, admin_zip)
    admin_shp = _unzip_shp(
        admin_zip, cache / "ne_admin1", "ne_10m_admin_1_states_provinces.shp"
    )

    places_zip = cache / "ne_places.zip"
    _download_first(NE_PLACES_URLS, places_zip)
    places_shp = _unzip_shp(
        places_zip, cache / "ne_places", "ne_10m_populated_places.shp"
    )

    return admin_shp, places_shp


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
        columns = spec["columns"]
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
            "INSERT INTO gpkg_geometry_columns VALUES (?, 'geom', ?, 4326, 0, 0)",
            (table, geom_type),
        )
        col_names = [c for c, _ in columns]
        placeholders = ", ".join("?" for _ in range(1 + len(col_names)))
        insert_sql = (
            f'INSERT INTO "{table}" (geom'
            + (", " + ", ".join(f'"{c}"' for c in col_names) if col_names else "")
            + f") VALUES ({placeholders})"
        )
        for feat in features:
            props = feat.get("properties") or {}
            row = [gpkg_geom_blob(feat["geometry"])]
            for c in col_names:
                row.append(props.get(c))
            cur.execute(insert_sql, row)

    conn.commit()
    conn.close()


def _centroid_of(geom: dict) -> tuple[float, float]:
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
        return (0.0, 0.0)
    return (sum(xs) / len(xs), sum(ys) / len(ys))


def _close_ring(ring: list[list[float]]) -> list[list[float]]:
    if len(ring) < 3:
        return ring
    if ring[0][0] != ring[-1][0] or ring[0][1] != ring[-1][1]:
        return ring + [ring[0]]
    return ring


def _polygon_shape_to_multipolygon(shape) -> dict | None:
    """Shapefile polygon parts → MultiPolygon (each part as one outer ring)."""
    if shape.shapeType not in (5, 15, 25, 31):  # polygon variants
        return None
    parts = list(shape.parts) + [len(shape.points)]
    polys: list[list] = []
    for i in range(len(parts) - 1):
        ring = [
            [float(p[0]), float(p[1])]
            for p in shape.points[parts[i] : parts[i + 1]]
        ]
        ring = _close_ring(ring)
        if len(ring) >= 4:
            polys.append([ring])
    if not polys:
        return None
    return {"type": "MultiPolygon", "coordinates": polys}


def _rec_str(rec: dict, *keys: str) -> str:
    for k in keys:
        v = rec.get(k)
        if v is None:
            continue
        if isinstance(v, bytes):
            v = v.decode("utf-8", errors="replace")
        s = str(v).strip()
        if s and s.lower() not in ("none", "null"):
            return s
    return ""


def _union_multipolygons(parts: list[dict], name: str, adcode: str) -> dict | None:
    """Concatenate NE admin parts into one MultiPolygon (overview dissolve)."""
    polys: list = []
    for a in parts:
        geom = a.get("geometry") or {}
        coords = geom.get("coordinates")
        if not coords:
            continue
        if geom.get("type") == "Polygon":
            polys.append(coords)
        elif geom.get("type") == "MultiPolygon":
            polys.extend(coords)
    if not polys:
        return None
    return {
        "geometry": {"type": "MultiPolygon", "coordinates": polys},
        "properties": {"name": name, "adcode": adcode},
    }


def build_area_features(admin_shp: Path) -> list[dict]:
    """Natural Earth admin_1 for mainland China + TW/HK/MO province-scale units.

    Taiwan / Hong Kong / Macao NE rows are county/district fragments — dissolve
    each to one overview silhouette. Drop Paracel (西沙) south of CHINA_BBOX.
    """
    shapefile = _require_pyshp()
    sf = shapefile.Reader(str(admin_shp))
    fields = [f[0] for f in sf.fields[1:]]
    raw: list[dict] = []
    for sr in sf.iterShapeRecords():
        rec = dict(zip(fields, sr.record))
        adm0 = _rec_str(rec, "adm0_a3", "ADM0_A3").upper()
        if not adm0:
            admin = _rec_str(rec, "admin", "ADMIN", "adm0_name", "ADM0_NAME")
            if admin == "China":
                adm0 = "CHN"
            elif admin == "Taiwan":
                adm0 = "TWN"
            elif admin in ("Hong Kong",):
                adm0 = "HKG"
            elif admin in ("Macao", "Macau"):
                adm0 = "MAC"
            else:
                continue
        if adm0 not in NE_ADMIN_ADM0:
            continue
        geom = _polygon_shape_to_multipolygon(sr.shape)
        if not geom:
            continue
        env = _envelope(geom)
        name = _rec_str(
            rec,
            "name_zh",
            "NAME_ZH",
            "name_local",
            "NAME_LOCAL",
            "name",
            "NAME",
            "gn_name",
            "GN_NAME",
        )
        if not name:
            name = _rec_str(rec, "adm1_code", "ADM1_CODE") or "unknown"
        adcode = _rec_str(rec, "adm1_code", "ADM1_CODE", "code_hasc", "CODE_HASC")
        if not adcode:
            adcode = f"{adm0}-{len(raw)}"
        raw.append(
            {
                "geometry": geom,
                "properties": {
                    "name": name,
                    "adcode": adcode,
                    "_adm0": adm0,
                    "_env": env,
                },
            }
        )

    chn: list[dict] = []
    twn: list[dict] = []
    hkg: list[dict] = []
    mac: list[dict] = []
    for a in raw:
        adm0 = a["properties"]["_adm0"]
        name = a["properties"]["name"]
        adcode = a["properties"]["adcode"]
        env = a["properties"]["_env"]
        if adm0 == "CHN":
            # Overview bbox is 18°N; Paracel/Xisha sits ~15.8–16.9°N.
            if env[3] < CHINA_BBOX[1] or "西沙" in name or adcode.startswith("PFA"):
                continue
            if len(name) < 2:
                continue
            chn.append(a)
        elif adm0 == "TWN":
            twn.append(a)
        elif adm0 == "HKG":
            hkg.append(a)
        elif adm0 == "MAC":
            mac.append(a)

    areas: list[dict] = []
    for a in chn:
        a["properties"].pop("_adm0", None)
        a["properties"].pop("_env", None)
        areas.append(a)
    dissolved = _union_multipolygons(twn, "台湾", "TWN")
    if dissolved:
        areas.append(dissolved)
    dissolved = _union_multipolygons(hkg, "香港", "HKG")
    if dissolved:
        areas.append(dissolved)
    dissolved = _union_multipolygons(mac, "澳门", "MAC")
    if dissolved:
        areas.append(dissolved)
    return areas


def build_point_text(places_shp: Path, areas: list[dict]) -> tuple[list[dict], list[dict]]:
    """Populated places in China bbox; fall back to area centroids if empty.

    Returns (points, texts). texts is always empty — city names live on the
    point layer only so MapLibre/default carto cannot double-draw labels.
    """
    shapefile = _require_pyshp()
    sf = shapefile.Reader(str(places_shp))
    fields = [f[0] for f in sf.fields[1:]]
    points: list[dict] = []
    texts: list[dict] = []
    seen: set[str] = set()

    # (priority, scalerank, name, x, y) — priority 0 = must-keep tier-1.
    candidates: list[tuple[int, int, str, float, float]] = []
    found_must: set[str] = set()
    for sr in sf.iterShapeRecords():
        rec = dict(zip(fields, sr.record))
        adm0 = _rec_str(rec, "ADM0_A3", "adm0_a3", "SOV0NAME", "sov0name").upper()
        # Coordinate from shape or LATITUDE/LONGITUDE fields.
        if sr.shape.points:
            x, y = float(sr.shape.points[0][0]), float(sr.shape.points[0][1])
        else:
            try:
                x = float(rec.get("LONGITUDE") or rec.get("longitude") or 0)
                y = float(rec.get("LATITUDE") or rec.get("latitude") or 0)
            except (TypeError, ValueError):
                continue
        if not _pt_in_bbox(x, y):
            continue
        # China / TW / HK / MO only — bbox also covers Seoul / Delhi / etc.
        try:
            scalerank = int(rec.get("SCALERANK") or rec.get("scalerank") or 99)
        except (TypeError, ValueError):
            scalerank = 99
        in_adm = adm0 in NE_ADMIN_ADM0 or "CHINA" in adm0 or "TAIWAN" in adm0
        if not in_adm:
            continue
        name_ascii = _rec_str(rec, "NAMEASCII", "nameascii", "NAME_EN", "name_en")
        name = _rec_str(
            rec,
            "NAME_ZH",
            "name_zh",
            "NAME",
            "name",
            "NAMEASCII",
            "nameascii",
        )
        if not name and not name_ascii:
            continue
        zh = _city_zh_alias(name, name_ascii)
        if zh:
            name = zh
            found_must.add(zh)
            priority = 0
        else:
            # Prefer province seats (low scalerank) over dense mid-tier towns.
            priority = 1 if scalerank <= 4 else 2
        if not name:
            continue
        candidates.append((priority, scalerank, name, x, y))

    # Seed any must-keep cities NE omitted (historically Beijing / Shanghai).
    for _key, (zh, lon, lat) in MUST_KEEP_CITIES.items():
        if zh in found_must:
            continue
        if not _pt_in_bbox(lon, lat):
            continue
        candidates.append((0, 0, zh, lon, lat))
        found_must.add(zh)

    candidates.sort(key=lambda t: (t[0], t[1], t[2]))
    for _prio, _scalerank, name, x, y in candidates:
        key = name.lower()
        if key in seen:
            continue
        seen.add(key)
        geom = {"type": "Point", "coordinates": [x, y]}
        points.append({"geometry": geom, "properties": {"name": name, "kind": "city"}})
        # Do NOT emit a parallel text layer at y+0.08 — default carto maps both
        # point and text to source-layer "label" and doubles city glyphs.
        if len(points) >= 64:
            break

    if points:
        return points, texts

    # Fallback: province centroids so the point layer stays non-empty.
    for area in areas:
        name = area["properties"]["name"]
        x, y = _centroid_of(area["geometry"])
        geom = {"type": "Point", "coordinates": [x, y]}
        points.append({"geometry": geom, "properties": {"name": name, "kind": "city"}})
    return points, texts


def _pt_in_bbox(
    x: float, y: float, bbox: tuple[float, float, float, float] = CHINA_BBOX
) -> bool:
    return bbox[0] <= x <= bbox[2] and bbox[1] <= y <= bbox[3]


def _clip_segment_to_bbox(
    coords: list, bbox: tuple[float, float, float, float] = CHINA_BBOX
) -> list[list]:
    """Keep contiguous runs of vertices inside bbox; drop foreign stubs."""
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


def _folded_token(s: str) -> str:
    return "".join(ch for ch in (s or "").lower() if ch.isalnum())


def _canonicalize_water_name(name: str, name_en: str = "", name_zh: str = "") -> str:
    """Map Latin / mixed water names onto product Chinese labels."""
    tokens = [_folded_token(s) for s in (name, name_en, name_zh) if s]
    for tok in tokens:
        if tok in YANGTZE_NAME_KEYS:
            return "长江"
        if tok in PEARL_NAME_KEYS:
            return "珠江"
        if tok in HUANG_NAME_KEYS:
            return "黄河"
    return (name_zh or name or name_en or "").strip()


def _geom_from_segments(segments: list[list]) -> dict:
    if len(segments) == 1:
        return {"type": "LineString", "coordinates": segments[0]}
    return {"type": "MultiLineString", "coordinates": segments}


def _segments_from_geom(geom: dict) -> list[list]:
    t = geom.get("type")
    coords = geom.get("coordinates") or []
    if t == "LineString":
        return [coords]
    if t == "MultiLineString":
        return list(coords)
    return []


def _merge_lines_named(lines: list[dict], name: str) -> list[dict]:
    """Collapse every feature with |name| into one MultiLineString."""
    kept: list[dict] = []
    segs: list[list] = []
    kind = "river"
    extra: dict | None = None
    for f in lines:
        props = f.get("properties") or {}
        if props.get("name") == name:
            segs.extend(_segments_from_geom(f["geometry"]))
            kind = props.get("kind") or kind
            extra = props
        else:
            kept.append(f)
    if not segs:
        return lines
    merged = {
        "geometry": _geom_from_segments(segs),
        "properties": {
            "name": name,
            "kind": kind,
            "class": (extra or {}).get("class") or "",
        },
    }
    return [merged] + kept


def _load_china_line_extract(path: Path) -> list[dict]:
    """Load the committed China river/road extract (EPSG:4326)."""
    if not path.is_file() or path.stat().st_size < 100:
        raise SystemExit(
            f"missing China line extract {path}. Rebuild does not download "
            "the global Natural Earth rivers or roads shapefiles."
        )
    fc = json.loads(path.read_text(encoding="utf-8"))
    out: list[dict] = []
    for feat in fc.get("features") or []:
        geom = feat.get("geometry")
        if not geom:
            continue
        props = feat.get("properties") or {}
        segments: list[list] = []
        for seg in _segments_from_geom(geom):
            segments.extend(_clip_segment_to_bbox(seg))
        if not segments:
            continue
        kind = str(props.get("kind") or "river")
        out.append(
            {
                "geometry": _geom_from_segments(segments),
                "properties": {
                    "name": str(props.get("name") or ""),
                    "kind": kind,
                    "class": str(props.get("class") or ""),
                },
            }
        )
    return out


def build_line_features(src: Path) -> list[dict]:
    """China hydrology from china_hydro.src.geojson — not the global NE rivers zip."""
    lines = _load_china_line_extract(src)
    for f in lines:
        name = f["properties"]["name"]
        canon = _canonicalize_water_name(name)
        if canon:
            f["properties"]["name"] = canon
        kind = f["properties"].get("kind") or "river"
        if kind not in ("river", "lake"):
            f["properties"]["kind"] = "river"
    lines = _merge_lines_named(lines, "长江")
    lines = _merge_lines_named(lines, "黄河")
    lines = _merge_lines_named(lines, "珠江")
    return lines


def build_road_features(src: Path) -> list[dict]:
    """China roads from china_roads.src.geojson — not the global NE roads zip."""
    roads = _load_china_line_extract(src)
    for f in roads:
        f["properties"]["kind"] = "road"
        cls = f["properties"].get("class") or "primary"
        if cls not in ("motorway", "trunk", "primary"):
            f["properties"]["class"] = "primary"
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


def write_land_outline(areas: list[dict], dest: Path) -> None:
    """MultiPolygon FeatureCollection for gdalwarp -cutline (same CRS as DEM)."""
    polys: list = []
    for a in areas:
        geom = a.get("geometry") or {}
        coords = geom.get("coordinates")
        if not coords:
            continue
        if geom.get("type") == "Polygon":
            polys.append(coords)
        elif geom.get("type") == "MultiPolygon":
            polys.extend(coords)
    if not polys:
        raise RuntimeError("no area polygons for land outline")
    fc = {
        "type": "FeatureCollection",
        "name": "china_outline",
        "crs": {
            "type": "name",
            "properties": {"name": "urn:ogc:def:crs:OGC:1.3:CRS84"},
        },
        "features": [
            {
                "type": "Feature",
                "properties": {"name": "china", "source": "natural_earth_10m_admin_1"},
                "geometry": {"type": "MultiPolygon", "coordinates": polys},
            }
        ],
    }
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(fc, ensure_ascii=False), encoding="utf-8")
    print(f"Wrote land outline {dest} ({len(polys)} polygons)", flush=True)


def run_dem_build(dem_out: Path, jobs: int = 12) -> int:
    """Invoke build_china_dem.py (real AWS tiles + NE cutline)."""
    script = Path(__file__).resolve().parent / "build_china_dem.py"
    cmd = [
        sys.executable,
        str(script),
        "--out",
        str(dem_out),
        "--source",
        "real",
        "--jobs",
        str(max(1, jobs)),
    ]
    print("+", " ".join(cmd), flush=True)
    return subprocess.call(cmd)


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
        default=REPO / "out" / "data",
        help="also copy finished gpkg/geojson/style here (default: out/data)",
    )
    ap.add_argument(
        "--with-dem",
        action="store_true",
        help="also build testing/data/china_dem.tif (EPSG:4326, NE land cutline)",
    )
    ap.add_argument(
        "--dem-out",
        type=Path,
        default=Path(__file__).resolve().parent / "china_dem.tif",
        help="DEM output path when --with-dem is set",
    )
    ap.add_argument(
        "--dem-jobs",
        type=int,
        default=12,
        help="parallel DEM tile download workers (default: 12)",
    )
    args = ap.parse_args()

    admin_shp, places_shp = fetch_sources(args.cache)
    print("Building area from Natural Earth admin_1…", flush=True)
    areas = build_area_features(admin_shp)
    print(f"  area features: {len(areas)}", flush=True)
    if len(areas) < 10:
        raise SystemExit(
            f"too few admin_1 areas ({len(areas)}); check ADM0 filter / shp fields"
        )

    # Land outline for DEM cutline (same polygons as area layer).
    outline_cache = args.cache / "china_outline.geojson"
    outline_data = Path(__file__).resolve().parent / "_china_ne_outline.geojson"
    write_land_outline(areas, outline_cache)
    write_land_outline(areas, outline_data)

    print("Building point/text from Natural Earth places…", flush=True)
    points, texts = build_point_text(places_shp, areas)
    print(f"  point/text: {len(points)}/{len(texts)}", flush=True)

    print("Building lines from China hydro extract…", flush=True)
    lines = build_line_features(CHINA_HYDRO_SRC)
    print(f"  river/lake features: {len(lines)}", flush=True)
    print("Building lines from China roads extract…", flush=True)
    roads = build_road_features(CHINA_ROADS_SRC)
    print(f"  road features: {len(roads)}", flush=True)
    lines = lines + roads
    print(f"  line features total: {len(lines)}", flush=True)

    layers = {
        "area": {
            "geom_type": "MULTIPOLYGON",
            "columns": [("name", "TEXT"), ("adcode", "TEXT")],
            "features": [
                {
                    "geometry": a["geometry"],
                    "properties": {
                        "name": a["properties"]["name"],
                        "adcode": a["properties"]["adcode"],
                    },
                }
                for a in areas
            ],
        },
        "line": {
            "geom_type": "GEOMETRY",
            "columns": [("name", "TEXT"), ("kind", "TEXT"), ("class", "TEXT")],
            "features": lines,
        },
        "point": {
            "geom_type": "POINT",
            "columns": [("name", "TEXT"), ("kind", "TEXT")],
            "features": points,
        },
        # Keep an empty text table for schema compatibility; names are on point.
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
        f"geojson_size={geojson_path.stat().st_size}\n"
        f"source=natural_earth_10m\n",
        encoding="utf-8",
    )
    print(f"Wrote {pin_path}", flush=True)

    if args.also_out_dir:
        args.also_out_dir.mkdir(parents=True, exist_ok=True)
        style_src = Path(__file__).resolve().parent / "china_city.style.json"
        license_src = Path(__file__).resolve().parent / "china_city.LICENSE.txt"
        for src in (out, geojson_path, style_src, license_src):
            if not src.is_file():
                continue
            dest = args.also_out_dir / src.name
            dest.write_bytes(src.read_bytes())
            print(f"Copied {dest}", flush=True)
        migrated = args.also_out_dir / "china_city.LICENSE_migrated.txt"
        migrated.write_text(
            "SUPERSEDED.\n"
            "This filename described a mixed Aliyun DataV (GCJ-02) admin pack "
            "plus Natural Earth rivers. The current china_city pack is Natural "
            "Earth 10m only, EPSG:4326 / CRS84. See china_city.LICENSE.txt.\n",
            encoding="utf-8",
        )
        print(f"Wrote superseded notice {migrated}", flush=True)

    print(
        "counts:",
        f"area={len(areas)} line={len(lines)} "
        f"(roads={len(roads)}) point={len(points)} text={len(texts)}",
        flush=True,
    )

    if args.with_dem:
        rc = run_dem_build(args.dem_out, jobs=args.dem_jobs)
        if rc != 0:
            return rc
        if args.also_out_dir and args.dem_out.exists():
            dem_dest = args.also_out_dir / args.dem_out.name
            dem_dest.write_bytes(args.dem_out.read_bytes())
            print(f"Copied {dem_dest}", flush=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
