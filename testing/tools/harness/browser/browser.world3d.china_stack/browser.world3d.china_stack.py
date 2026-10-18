# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# China planar stack: DEM, ocean, fog, sky, cloud. Fail-closed ladder.

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
    from steps.scene.stack import china_stack

    china_stack(gp)
