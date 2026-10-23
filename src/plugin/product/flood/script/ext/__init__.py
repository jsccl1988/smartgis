# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.flood_levels — area curve over flood.level_mask."""

import json
import os
import struct


def _need(tags, tag, what):
    if tag not in tags:
        raise ValueError("mask tiff missing " + what)
    return tags[tag]


def _tiff_values(data, endian, tags, tag):
    typ, count, raw, inline_at = _need(tags, tag, "tag %d" % tag)
    size = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 12: 8}.get(typ)
    if size is None:
        raise ValueError("unsupported tiff type %d" % typ)
    nbytes = size * count
    offset = inline_at if nbytes <= 4 else raw
    out = []
    for i in range(count):
        pos = offset + i * size
        if typ in (1, 2):
            out.append(data[pos])
        elif typ == 3:
            out.append(int.from_bytes(data[pos : pos + 2], endian))
        elif typ == 4:
            out.append(int.from_bytes(data[pos : pos + 4], endian))
        elif typ == 12:
            fmt = "<d" if endian == "little" else ">d"
            out.append(struct.unpack_from(fmt, data, pos)[0])
        else:
            raise ValueError("unsupported tiff type %d" % typ)
    return out


def read_mask_grid(path):
    """Uncompressed classic GeoTIFF byte mask from GDAL's default GTiff writer."""
    with open(path, "rb") as handle:
        data = handle.read()
    if len(data) < 8:
        raise ValueError("mask tiff is too small")
    if data[0:2] == b"II":
        endian = "little"
    elif data[0:2] == b"MM":
        endian = "big"
    else:
        raise ValueError("mask is not a TIFF")
    if int.from_bytes(data[2:4], endian) != 42:
        raise ValueError("mask is not a classic TIFF")
    ifd = int.from_bytes(data[4:8], endian)
    count = int.from_bytes(data[ifd : ifd + 2], endian)
    tags = {}
    for i in range(count):
        entry = ifd + 2 + i * 12
        tag = int.from_bytes(data[entry : entry + 2], endian)
        typ = int.from_bytes(data[entry + 2 : entry + 4], endian)
        n = int.from_bytes(data[entry + 4 : entry + 8], endian)
        raw = int.from_bytes(data[entry + 8 : entry + 12], endian)
        tags[tag] = (typ, n, raw, entry + 8)
    if 322 in tags:
        raise ValueError("tiled mask tiff is not supported")
    compression = _tiff_values(data, endian, tags, 259)[0]
    if compression != 1:
        raise ValueError("compressed mask tiff is not supported")
    bits = _tiff_values(data, endian, tags, 258)[0]
    if bits != 8:
        raise ValueError("mask tiff is not 8-bit")
    if 277 in tags and _tiff_values(data, endian, tags, 277)[0] != 1:
        raise ValueError("mask tiff is not single-band")
    width = _tiff_values(data, endian, tags, 256)[0]
    height = _tiff_values(data, endian, tags, 257)[0]
    scale = _tiff_values(data, endian, tags, 33550)
    if len(scale) < 2 or scale[0] == 0 or scale[1] == 0:
        raise ValueError("mask tiff has no pixel scale")
    offsets = _tiff_values(data, endian, tags, 273)
    counts = _tiff_values(data, endian, tags, 279)
    if len(offsets) != len(counts):
        raise ValueError("mask tiff strips are inconsistent")
    grid = bytearray()
    for off, nbytes in zip(offsets, counts):
        grid += data[off : off + nbytes]
    need = width * height
    if len(grid) < need:
        raise ValueError("mask tiff raster is short")
    return width, height, abs(scale[0]), abs(scale[1]), bytes(grid[:need])


def mask_area(path):
    """Flooded cell count times pixel area in the DEM map units."""
    width, height, pixel_w, pixel_h, grid = read_mask_grid(path)
    cells = sum(1 for value in grid if value)
    return {
        "width": width,
        "height": height,
        "flooded_cells": cells,
        "pixel_width": pixel_w,
        "pixel_height": pixel_h,
        "area_map": cells * pixel_w * pixel_h,
    }


def _write_table(out_dir, table):
    json_path = os.path.join(out_dir, "curve.json")
    csv_path = os.path.join(out_dir, "curve.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(table, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "water_level,flooded_cells,pixel_width,pixel_height,"
            "area_map,mask,selected\n"
        )
        for row in table["rows"]:
            handle.write(
                "%s,%d,%.8g,%.8g,%.8g,%s,%s\n"
                % (
                    row["water_level"],
                    row["flooded_cells"],
                    row["pixel_width"],
                    row["pixel_height"],
                    row["area_map"],
                    row["mask"],
                    "1" if row["selected"] else "0",
                )
            )
    return json_path, csv_path


def level_curve(host, spec):
    """Run one mask per water level, write the area table, present selected."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    dem = spec.get("dem") or ""
    out_dir = spec.get("out_dir") or ""
    levels = spec.get("levels")
    if not dem or not os.path.isfile(dem):
        return {"ok": False, "error": "DEM 不存在"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(levels, list) or not levels:
        return {"ok": False, "error": "levels 必须是非空水位数组"}
    try:
        seed_x = float(spec["seed_x"])
        seed_y = float(spec["seed_y"])
        selected = float(spec["selected"])
        levels = [float(level) for level in levels]
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "seed_x、seed_y、levels、selected 必须是数"}
    if not any(abs(level - selected) <= 1e-6 for level in levels):
        return {"ok": False, "error": "selected 必须是 levels 中的一档水位"}

    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for index, level in enumerate(levels):
        mask_path = os.path.join(out_dir, "mask_%02d.tif" % index)
        payload = json.dumps(
            {
                "dem": dem,
                "output": mask_path,
                "seed_x": seed_x,
                "seed_y": seed_y,
                "water_level": level,
                "frames": 1,
            }
        )
        if not host.run_processing("flood.level_mask", payload):
            return {
                "ok": False,
                "error": "水位 %s 的掩膜计算失败" % level,
            }
        try:
            area = mask_area(mask_path)
        except (OSError, ValueError) as exc:
            return {"ok": False, "error": "无法读取掩膜面积：" + str(exc)}
        rows.append(
            {
                "water_level": level,
                "flooded_cells": area["flooded_cells"],
                "pixel_width": area["pixel_width"],
                "pixel_height": area["pixel_height"],
                "area_map": area["area_map"],
                "mask": mask_path,
                "selected": abs(level - selected) <= 1e-6,
            }
        )

    chosen = next(row for row in rows if row["selected"])
    table = {
        "dem": dem,
        "seed_x": seed_x,
        "seed_y": seed_y,
        "selected": selected,
        "area_unit": "map",
        "rows": rows,
    }
    json_path, csv_path = _write_table(out_dir, table)
    present = json.dumps(
        {
            "dem": dem,
            "output": chosen["mask"],
            "seed_x": seed_x,
            "seed_y": seed_y,
            "water_level": selected,
            "frames": 1,
        }
    )
    if not host.run_processing("flood.inundate", present):
        return {
            "ok": False,
            "error": "对照表已写入 %s，选定水位掩膜未能呈现" % json_path,
            "table": json_path,
        }
    summary = (
        "水位 %s 淹没面积 %s（地图单位）。对照表 %s"
        % (selected, chosen["area_map"], json_path)
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
        "selected_area": chosen["area_map"],
    }
