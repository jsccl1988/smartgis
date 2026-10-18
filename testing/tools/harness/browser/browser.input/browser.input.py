# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Input digitize path (marks gate for suite "browser.input").
# Opens china_city, then the shared point/line/polygon clicks.
# No m0 round-trip, selection tools, or fail_rc (those stay on steps.edit.roundtrip.edit_m0).

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
    from steps.edit.digitize import append_line, append_point, append_polygon
    from steps.session.china import open_china_city

    gp.mark("show")
    gp.pump(ms=300)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.select_map_tab(0)
    gp.pump(ms=400)
    open_china_city(gp)
    gp.fit_extent()
    gp.wait_viewport(face="map", ms=30000, frame=0)
    gp.wait_ready(ms=5000, soft=1)
    gp.mark("map-ready")
    gp.require_edit_tool_session()
    gp.mark("host-ok")
    append_point(gp)
    gp.mark("input-point-ok")
    append_line(gp)
    gp.mark("input-line-ok")
    append_polygon(gp)
    gp.mark("input-poly-ok")
    gp.mark("input-ok")
    gp.mark("pass")
