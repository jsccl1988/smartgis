# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Fail-closed China planar ladder: DEM, ocean, fog, sky, cloud.
# Later stages stay off until their step. GPU warmup stays in export_bmp.

import json

from steps.capture.export import capture_frame
from steps.scene.shell import finish_orbit, open_scene

# Cumulative masks. Earlier passes stay on; later passes stay off.
_STAGES = (
    ("dem", {"sky": False, "ocean": False, "cloud": False, "fog": False}),
    ("ocean", {"sky": False, "ocean": True, "cloud": False, "fog": False}),
    ("fog", {"sky": False, "ocean": True, "cloud": False, "fog": True}),
    ("sky", {"sky": True, "ocean": True, "cloud": False, "fog": True}),
    ("cloud", {"sky": True, "ocean": True, "cloud": True, "fog": True}),
)


def _leaf(name):
    if name == "cloud":
        return "atmosphere-showcase-china-stack-cloud.bmp"
    return "china-stack-%s.bmp" % name


def china_stack(sg):
    open_scene(sg)
    sg.run_processing(id="world3d.apply_look", args=json.dumps({"mode": "land"}))
    for name, flags in _STAGES:
        sg.run_processing(
            id="world3d.set_atmosphere", args=json.dumps(flags)
        )
        capture_frame(sg, _leaf(name), "scene3d")
        sg.mark("china-stack-%s-ok" % name)
    sg.mark("bmp-ok")
    finish_orbit(sg, "orbit", 1600, 20)
