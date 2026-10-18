# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Map showcase shell. Suites keep timer, invalidate, and fps differences.


def attach_plugins(sg):
    sg.require_plugins()


def open_shell(sg, wait_ms=30000):
    sg.mark("show")
    sg.pump(ms=400)
    sg.require_hwnd()
    sg.mark("hwnd-ok")
    sg.suppress_dialogs(on=1)
    sg.wait_viewport(face="map", ms=wait_ms, frame=0)
    sg.mark("map-ready")
    attach_plugins(sg)
    sg.select_map_tab(0)
    sg.pump(ms=400)
