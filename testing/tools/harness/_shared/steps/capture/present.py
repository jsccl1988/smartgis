# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Orbit yaw is expect_orbit_moved (already run by the suite before this step).
# Scene GPU present stays on the scene3d path.


def present(sg):
    sg.expect_orbit_moved()
    sg.mark("flycube-skipped")


def main():
    import glang.plugin as gp

    present(gp)
