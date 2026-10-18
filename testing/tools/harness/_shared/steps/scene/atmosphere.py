# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Atmosphere showcase order: apply_look, optional fly_globe, scene3d BMP, orbit.

import json

from steps.capture.export import capture_frame
from steps.scene.shell import finish_orbit, open_scene


def show(sg, mode, leaf, fly_mode, fly_ms, fly_steps, before_export=None):
    open_scene(sg)
    sg.run_processing(id="world3d.apply_look", args=json.dumps({"mode": mode}))
    if before_export is not None:
        before_export(sg)
    capture_frame(sg, leaf, "scene3d")
    sg.mark("bmp-ok")
    finish_orbit(sg, fly_mode, fly_ms, fly_steps)


# Planar looks share one scene open. full is last so the suite BMP stays full.
_PLANAR = (
    ("land", "atmosphere-showcase-land.bmp"),
    ("ocean", "atmosphere-showcase-ocean.bmp"),
    ("coast", "atmosphere-showcase-coast.bmp"),
    ("full", "atmosphere-showcase-full.bmp"),
)


def _look_export(sg, mode, leaf):
    ok = sg.run_processing(id="world3d.apply_look", args=json.dumps({"mode": mode}))
    if ok is False or ok == 0:
        raise RuntimeError("world3d.apply_look failed: %s" % mode)
    capture_frame(sg, leaf, "scene3d")
    sg.mark("bmp-%s-ok" % mode)


def full(sg):
    """Land, ocean, coast, then full. One orbit after the full export."""
    open_scene(sg)
    for mode, leaf in _PLANAR:
        _look_export(sg, mode, leaf)
    sg.mark("bmp-ok")
    finish_orbit(sg, "orbit", 1600, 20)


def legacy(sg):
    show(sg, "legacy", "atmosphere-showcase-legacy.bmp", "orbit", 1600, 20)


def _fly_globe(sg):
    sg.run_processing(id="world3d.fly_globe", args=json.dumps({"t": 1}))


def globe(sg):
    show(
        sg,
        "globe",
        "atmosphere-showcase-globe.bmp",
        "spherical",
        2400,
        24,
        _fly_globe,
    )
