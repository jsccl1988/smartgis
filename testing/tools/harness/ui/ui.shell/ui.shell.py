# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Shell chrome BMP path (suite "ui.shell").

import glang.plugin as gp

def main():
    gp.mark("shell")
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
    gp.pump(ms=500)
    gp.mark("interact-pre")
    gp.apply_scenario_panels("shell")
    gp.mark("interact-post")
    gp.layout_gate("shell")
    gp.ui_present_capture("shell")
