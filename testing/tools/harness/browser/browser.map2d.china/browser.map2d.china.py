# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# China carto showcase. Seed + export_bmp(frame=china_product).
# Warm cache, visible-signal gate, GPU smoke, and FPS live in glang export.
# Differs from align/orthogrid: timers around seed, then fps_bench.

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
    gp.stop_map_timers()
    seed_mode(gp, "china", "china-ok")
    export_seed_bmp(gp, "map2d-showcase-china.bmp", "china_product")
    gp.resume_map_timers()
    gp.pump(ms=200)
    gp.fps_bench()
    gp.mark("pass")
    gp.stop_map_timers()
