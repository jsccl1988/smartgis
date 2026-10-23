# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# World preview and True-Earth browse. Picture is export_bmp(frame="scene3d").

import json

from steps.capture.export import capture_frame
from steps.scene.shell import finish_orbit, open_scene


def world_preview(sg):
    open_scene(sg)
    sg.run_processing(id="world3d.open_earth", args="{}")
    sg.run_processing(id="world3d.apply_look", args=json.dumps({"mode": "globe"}))
    capture_frame(sg, "plugin-showcase-world-preview.bmp", "scene3d")
    sg.mark("export-ok")
    sg.mark("pass")


def earth_browse(sg):
    open_scene(sg)
    sg.run_processing(id="world3d.open_earth", args="{}")
    sg.mark("earth-ok")
    sg.fit_scene_box()
    sg.pump(ms=200)
    sg.mark("fit-box-ok")
    sg.camera_fly(mode="spherical", ms=2400, steps=24)
    sg.mark("camera-fly-ok")
    capture_frame(sg, "plugin-showcase-world3d.bmp", "scene3d")
    sg.mark("bmp-ok")
    sg.mark("pass")
