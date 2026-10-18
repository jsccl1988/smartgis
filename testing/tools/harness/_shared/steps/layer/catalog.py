# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Catalog stand-in: layer JSON, china_city counts, style file, BMP export.
# Tile-provider HDC paint stays in the product presenter, not this script.

import json
import os

from steps.capture.export import capture_frame
from steps.capture.print_page import compose_page, print_style_path
from steps.probe import require_count
from steps.session.china import resolve_china_city


def layer_names(raw):
    try:
        rows = json.loads(raw or "[]")
    except json.JSONDecodeError:
        return []
    names = []

    def walk(node):
        if isinstance(node, dict):
            if "name" in node:
                names.append(str(node["name"]))
            for child in node.get("children") or []:
                walk(child)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(rows)
    return names


def layers_m1(sg):
    sg.pump(ms=50)
    require_count(sg, 2, "layers expected the edit round-trip features")
    names = layer_names(sg.list_layers())
    if not names:
        raise RuntimeError("layer list empty")
    sg.mark("layers-ok")

    china = resolve_china_city(sg)
    sg.open_map(path=china, fail_rc=26)
    require_count(sg, 200, "china_city feature count")
    names = layer_names(sg.list_layers())
    if len(names) < 3 or "point" not in names:
        raise RuntimeError("china_city layers")
    sg.mark("ogr-ok")
    sg.mark("china-city-ok")
    sg.mark("m1-labels-ok")

    style = print_style_path()
    if os.path.isfile(style):
        sg.apply_style_file(path=style)
    else:
        m1style = sg.resolve_data(kind="data", leaf="china_city.style.json")
        if not m1style:
            raise RuntimeError("china_city.style.json missing")
        sg.apply_style_file(path=m1style)
    if sg.style_has_document() != "1":
        raise RuntimeError("style document missing")
    sg.mark("m1-style-ok")

    capture_frame(sg, "smartgis_m1_selftest.bmp", "document_extent")
    sg.mark("m1-export-ok")
    page = sg.capture_path(leaf="smartgis_m1_layout.bmp")
    mapped = sg.capture_path(leaf="smartgis_m1_selftest.bmp")
    if compose_page(mapped, page):
        sg.mark("m1-layout-ok")
    sg.fit_extent()


def main():
    import glang.plugin as gp

    layers_m1(gp)
