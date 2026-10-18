# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Shared chrome for ui.interact / .os / .smoke / .combo.
# apply_scenario_panels("interact") runs UI_INTERACT_GESTURE_SCRIPT (or
# harness ui.interact/ui.interact.py).

import glang.plugin as gp

def main():
    gp.mark("interact")
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
    gp.apply_scenario_panels("interact")
    gp.pump(200)
    gp.mark("interact-post")
    gp.layout_gate("interact")
    gp.ui_present_capture("interact")
