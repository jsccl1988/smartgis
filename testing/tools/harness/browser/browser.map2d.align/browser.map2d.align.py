# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# StyleDocument align showcase. Seed + export_bmp(frame=map2d_china).
# Warm cache, visible-signal gate, and GPU smoke live in glang export.
# Differs from china: invalidate before export, no fps_bench, timers stay up.

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
    from steps.capture.export import export_seed_bmp
    from steps.session.seed import seed_mode
    from steps.session.shell import open_shell

    open_shell(gp)
    seed_mode(gp, "align", "china-ok")
    gp.pump(ms=300)
    gp.invalidate_map2d()
    gp.pump(ms=200)
    export_seed_bmp(gp, "map2d-showcase-align.bmp", "map2d_china")
    gp.mark("pass")
