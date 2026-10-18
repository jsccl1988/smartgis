# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# World preview dock integration. Payload: world3d.open_earth + globe look.

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
    from steps.scene.preview import world_preview

    world_preview(gp)
