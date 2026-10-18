# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Map Data tab chrome BMP (suite "ui.data").

import glang.plugin as gp

def main():
    gp.mark("data")
    gp.apply_ui_theme()
    gp.mark("theme-ok")
    gp.mark("pump-pre")
    gp.pump(600)
    gp.mark("pump-post")
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    china = gp.resolve_data(kind="data", leaf="china_city.gpkg")
    if not china:
        gp.mark("china-seed-miss")
        raise RuntimeError("china seed missing")
    gp.open_map(path=china)
    gp.fit_extent()
    gp.mark("interact-pre")
    gp.apply_scenario_panels("data")
    gp.mark("interact-post")
    gp.layout_gate("data")
    gp.ui_present_capture("data")
