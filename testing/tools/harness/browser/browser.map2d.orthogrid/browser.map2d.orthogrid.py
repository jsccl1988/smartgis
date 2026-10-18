# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Orthogrid map2d showcase. Seed + export_bmp(frame=map2d_orthogrid).
# Warm cache, visible-signal gate, and GPU smoke live in glang export.
# Differs from china: seed mark is orthogrid-ok; timers stop after seed.

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
    seed_mode(gp, "orthogrid", "orthogrid-ok")
    gp.stop_map_timers()
    export_seed_bmp(gp, "map2d-showcase-orthogrid.bmp", "map2d_orthogrid")
    gp.mark("pass")
