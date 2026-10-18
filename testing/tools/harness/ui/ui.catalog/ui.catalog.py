# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Catalog Maps page chrome BMP (suite "ui.catalog").

import glang.plugin as gp

def main():
    gp.mark("catalog")
    gp.apply_ui_theme()
    gp.mark("theme-ok")
    gp.mark("pump-pre")
    gp.pump(600)
    gp.mark("pump-post")
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.mark("interact-pre")
    gp.apply_scenario_panels("catalog")
    gp.catalog_tab(index=2)
    gp.pump(ms=200)
    gp.mark("catalog-maps")
    gp.mark("interact-post")
    gp.layout_gate("catalog")
    gp.catalog_tab(index=2)
    gp.pump(ms=120)
    gp.ui_present_capture("catalog")
