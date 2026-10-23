# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.map2d_coverage — tile span and scale bar for one bbox.

Tile indices follow content::tile::tiles_for_extent (full count, not the
prefetch cap inside map2d.open_pyramid). The scale bar follows
plugin::layout_furniture. Neither helper is a registered command, so the
numbers are computed here and only the selected pyramid is opened.
"""

import json
import math
import os


_EARTH_RADIUS_M = 6378137.0
_MAX_MERCATOR_LAT = 85.0511287798066
_MAX_ZOOM = 22
_PI = math.pi


def _clamp_index(index, count):
    if count <= 0:
        return 0
    if index < 0:
        return 0
    if index >= count:
        return count - 1
    return index


def _clamp_zoom(zoom):
    if zoom < 0:
        return 0
    if zoom > _MAX_ZOOM:
        return _MAX_ZOOM
    return zoom


def _clamp_lat(lat):
    if lat < -_MAX_MERCATOR_LAT:
        return -_MAX_MERCATOR_LAT
    if lat > _MAX_MERCATOR_LAT:
        return _MAX_MERCATOR_LAT
    return lat


def _lround(value):
    return int(math.floor(value + 0.5))


def _lonlat_to_tile(lon, lat, zoom):
    zoom = _clamp_zoom(zoom)
    count = 1 << zoom
    lon = max(-180.0, min(180.0, lon))
    lat = _clamp_lat(lat)
    lat_rad = lat * _PI / 180.0
    x = (lon + 180.0) / 360.0 * count
    y = (
        (1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / _PI)
        / 2.0
        * count
    )
    return (
        zoom,
        _clamp_index(int(math.floor(x)), count),
        _clamp_index(int(math.floor(y)), count),
    )


def _tile_span(west, south, east, north, zoom):
    """Inclusive XYZ range overlapping the bbox. Same corners as tiles_for_extent."""
    west_c = max(-180.0, min(180.0, min(west, east)))
    east_c = max(-180.0, min(180.0, max(west, east)))
    south_c = _clamp_lat(min(south, north))
    north_c = _clamp_lat(max(south, north))
    zoom = _clamp_zoom(zoom)
    _, x_nw, y_nw = _lonlat_to_tile(west_c, north_c, zoom)
    _, x_se, y_se = _lonlat_to_tile(min(east_c, 179.999999), south_c, zoom)
    x0 = min(x_nw, x_se)
    x1 = max(x_nw, x_se)
    y0 = min(y_nw, y_se)
    y1 = max(y_nw, y_se)
    return {
        "x0": x0,
        "x1": x1,
        "y0": y0,
        "y1": y1,
        "tile_count": (x1 - x0 + 1) * (y1 - y0 + 1),
    }


def _resolution_m_per_px(zoom, lat, tile_px=256):
    zoom = _clamp_zoom(zoom)
    if tile_px < 1:
        tile_px = 256
    lat_rad = _clamp_lat(lat) * _PI / 180.0
    circumference = 2.0 * _PI * _EARTH_RADIUS_M
    world_px = float(tile_px) * float(1 << zoom)
    return math.cos(lat_rad) * circumference / world_px


def _ground_meters(west, south, east, north):
    span = abs(east - west)
    if span <= 0.0:
        return 0.0
    lat = _clamp_lat((south + north) * 0.5)
    lat_rad = lat * _PI / 180.0
    return math.cos(lat_rad) * 2.0 * _PI * _EARTH_RADIUS_M * (span / 360.0)


def _nice_step(value):
    if value <= 0.0:
        return 1.0
    exp = math.floor(math.log10(value))
    base = 10.0 ** exp
    norm = value / base
    if norm < 1.5:
        nice = 1.0
    elif norm < 3.5:
        nice = 2.0
    elif norm < 7.5:
        nice = 5.0
    else:
        nice = 10.0
    return nice * base


def _scale_bar(west, south, east, north, width_px, height_px, dpi):
    """Match layout_furniture: ground width, denominator, and one bar."""
    ground = _ground_meters(west, south, east, north)
    empty = {
        "valid": False,
        "ground_width_m": ground,
        "scale_denominator": 0.0,
        "bar_meters": 0.0,
        "bar_len_px": 0,
    }
    if width_px < 80 or height_px < 80:
        return empty
    meters_per_px = ground / float(width_px)
    if meters_per_px <= 0.0:
        return empty
    if dpi <= 1.0:
        dpi = 96.0
    paper_m_per_px = 0.0254 / dpi
    denominator = meters_per_px / paper_m_per_px
    bar_meters = _nice_step(meters_per_px * 96.0)
    bar_len = max(24, _lround(bar_meters / meters_per_px))
    inner_w = (width_px - 16) - 8
    if bar_len > inner_w / 2:
        bar_len = max(24, inner_w // 3)
        bar_meters = meters_per_px * float(bar_len)
    return {
        "valid": True,
        "ground_width_m": ground,
        "scale_denominator": denominator,
        "bar_meters": bar_meters,
        "bar_len_px": bar_len,
    }


def _scale_ratio_label(denominator):
    if denominator <= 1.0:
        return "1:1"
    nice = _nice_step(denominator)
    return "1:%.0f" % nice


def _scale_bar_label(meters):
    if meters >= 1000.0:
        km = meters / 1000.0
        if km >= 10.0:
            return "%.0f km" % km
        return "%.1f km" % km
    return "%.0f m" % meters


def _write_table(out_dir, table):
    json_path = os.path.join(out_dir, "coverage.json")
    csv_path = os.path.join(out_dir, "coverage.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(table, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "z,x0,x1,y0,y1,tile_count,resolution_m_per_px,"
            "ground_width_m,scale_denominator,bar_meters,bar_len_px,selected\n"
        )
        for row in table["rows"]:
            handle.write(
                "%d,%d,%d,%d,%d,%d,%.8g,%.8g,%.8g,%.8g,%d,%s\n"
                % (
                    row["z"],
                    row["x0"],
                    row["x1"],
                    row["y0"],
                    row["y1"],
                    row["tile_count"],
                    row["resolution_m_per_px"],
                    row["ground_width_m"],
                    row["scale_denominator"],
                    row["bar_meters"],
                    row["bar_len_px"],
                    "1" if row["selected"] else "0",
                )
            )
    return json_path, csv_path


def _as_float(value, name):
    try:
        return float(value)
    except (TypeError, ValueError):
        raise ValueError(name + " 必须是数")


def coverage(host, spec):
    """Write one row per zoom, then open the selected pyramid once."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    template = spec.get("template") or ""
    out_dir = spec.get("out_dir") or ""
    zooms = spec.get("zooms")
    if "{z}" not in template or "{x}" not in template or "{y}" not in template:
        return {"ok": False, "error": "template 必须包含 {z}/{x}/{y}"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(zooms, list) or not zooms:
        return {"ok": False, "error": "zooms 必须是非空数组"}
    try:
        west = _as_float(spec["west"], "west")
        south = _as_float(spec["south"], "south")
        east = _as_float(spec["east"], "east")
        north = _as_float(spec["north"], "north")
        selected = int(spec["selected"])
        parsed = []
        for zoom in zooms:
            if isinstance(zoom, bool) or not isinstance(zoom, (int, float)):
                raise ValueError("zooms")
            if int(zoom) != zoom:
                raise ValueError("zooms")
            parsed.append(int(zoom))
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "范围、zooms 和 selected 必须是数"}
    if not (east > west and north > south):
        return {"ok": False, "error": "范围必须是 west<east 且 south<north"}
    if selected not in parsed:
        return {"ok": False, "error": "selected 必须是 zooms 中的一级"}
    try:
        width_px = int(spec.get("width_px", 1280))
        height_px = int(spec.get("height_px", 720))
        dpi = float(spec.get("dpi", 96))
    except (TypeError, ValueError):
        return {"ok": False, "error": "width_px、height_px、dpi 必须是数"}
    if width_px < 1 or height_px < 1:
        return {"ok": False, "error": "视图像素必须为正"}

    os.makedirs(out_dir, exist_ok=True)
    bar = _scale_bar(west, south, east, north, width_px, height_px, dpi)
    rows = []
    for zoom in parsed:
        span = _tile_span(west, south, east, north, zoom)
        mid_lat = (south + north) * 0.5
        rows.append(
            {
                "z": zoom,
                "x0": span["x0"],
                "x1": span["x1"],
                "y0": span["y0"],
                "y1": span["y1"],
                "tile_count": span["tile_count"],
                "resolution_m_per_px": _resolution_m_per_px(zoom, mid_lat),
                "ground_width_m": bar["ground_width_m"],
                "scale_denominator": bar["scale_denominator"],
                "bar_meters": bar["bar_meters"],
                "bar_len_px": bar["bar_len_px"],
                "selected": zoom == selected,
            }
        )
    chosen = next(row for row in rows if row["selected"])
    table = {
        "template": template,
        "west": west,
        "south": south,
        "east": east,
        "north": north,
        "width_px": width_px,
        "height_px": height_px,
        "dpi": dpi,
        "selected": selected,
        "ground_width_m": bar["ground_width_m"],
        "scale_denominator": bar["scale_denominator"],
        "scale_label": _scale_ratio_label(bar["scale_denominator"]),
        "bar_label": _scale_bar_label(bar["bar_meters"]),
        "bar_meters": bar["bar_meters"],
        "bar_len_px": bar["bar_len_px"],
        "furniture_valid": bar["valid"],
        "rows": rows,
    }
    json_path, csv_path = _write_table(out_dir, table)
    # open_pyramid reads template and the bbox. z is recorded for the caller;
    # the native command picks the prefetch zoom from the span.
    present = json.dumps(
        {
            "template": template,
            "west": west,
            "south": south,
            "east": east,
            "north": north,
            "z": selected,
        },
        ensure_ascii=False,
    )
    if not host.run_processing("map2d.open_pyramid", present):
        return {
            "ok": False,
            "error": "对照表已写入 %s，map2d.open_pyramid 未能打开" % json_path,
            "table": json_path,
            "csv": csv_path,
        }
    summary = (
        "z=%d 覆盖 %d 块瓦片，地面宽度 %.0f m，比例尺 %s。对照表 %s"
        % (
            selected,
            chosen["tile_count"],
            bar["ground_width_m"],
            table["scale_label"],
            json_path,
        )
    )
    report = dict(table)
    report["table"] = json_path
    report["csv"] = csv_path
    report["summary"] = summary
    return {
        "ok": True,
        "summary": summary,
        "table": json_path,
        "csv": csv_path,
        "report_json": json.dumps(report, ensure_ascii=False),
        "selected_tile_count": chosen["tile_count"],
        "ground_width_m": bar["ground_width_m"],
        "scale_denominator": bar["scale_denominator"],
        "bar_meters": bar["bar_meters"],
        "bar_len_px": bar["bar_len_px"],
    }
