# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Planar atmosphere sweep (land, ocean, coast, full) in one process.
# Payload: world3d.scenario.atmosphere.full. Suite BMP is the full look.

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
    from steps.scene.atmosphere import full

    full(gp)
