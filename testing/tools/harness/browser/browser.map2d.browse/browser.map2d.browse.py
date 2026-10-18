# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Map2d browse stress (integration). Same pan/stress/wheel clicks as navigate.
# Keeps china_city open, a still, stress count 32, and fps_bench.

import os
import sys

import glang.plugin as gp


def _steps():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.abspath(os.path.join(here, "..", "..", "_shared"))
    if path not in sys.path:
        sys.path.insert(0, path)


def main():
    _steps()
    from steps.session.china import open_china_city
    from steps.view.navigate import browse_burst, pan_drag, wheel_cursor

    gp.pump(ms=300)
    gp.require_hwnd()
    gp.mark("show")
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.select_map_tab(0)
    gp.pump(ms=400)
    open_china_city(gp)
    gp.fit_extent()
    gp.pump(ms=200)
    gp.wait_viewport(face="map", ms=30000, frame=0)
    gp.stop_map_timers()
    gp.capture_browse_still(face="map2d")
    gp.resume_map_timers()
    gp.pump(ms=100)
    gp.wait_ready(ms=5000, soft=1)
    pan_drag(gp)
    gp.mark("browse-pre-stress")
    browse_burst(gp, 32)
    wheel_cursor(gp)
    gp.fps_bench()
    gp.mark("browse-exit")
    gp.mark("pass")
    gp.stop_map_timers()
