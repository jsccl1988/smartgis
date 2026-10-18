# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Globe atmosphere. Payload: world3d.scenario.atmosphere.globe.
# Extra beat: world3d.fly_globe before the scene BMP, then a spherical orbit.

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
    from steps.scene.atmosphere import globe

    globe(gp)
