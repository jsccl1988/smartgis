# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Sequence and BMP compose live in _shared/steps/capture/print_page.py.

import os
import sys

import glang.plugin as gp

def _harness():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.abspath(os.path.join(here, "..", "..", "_shared"))
    if path not in sys.path:
        sys.path.insert(0, path)

def main():
    _harness()
    from steps.capture.print_page import print_layout

    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.wait_viewport(face="map", ms=30000, frame=0)
    gp.mark("map-ready")
    gp.require_plugins()
    print_layout(gp)
    gp.fit_extent()
    gp.pump(ms=300)
    gp.invalidate_map2d()
    gp.pump(ms=200)
    gp.mark("extent-ok")
