# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# HWND / plugin pipeline (suite "browser.harness").
# GIS steps live in testing/tools/harness/_shared/steps/.
# present stays after orbit-ok because it checks orbit yaw.

import os
import sys

import glang.plugin as gp

def _harness():
    here = os.path.dirname(os.path.abspath(__file__))
    path = os.path.abspath(os.path.join(here, "..", "..", "_shared"))
    if path not in sys.path:
        sys.path.insert(0, path)

def main():
    _harness()
    from steps.capture.present import present
    from steps.edit.process import milestones
    from steps.edit.roundtrip import edit_m0
    from steps.layer.catalog import layers_m1
    from steps.view.navigate import navigate

    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.wait_viewport(face="map", ms=90000, frame=1)
    gp.mark("map-frame-ok")
    gp.mark("map-ready")
    gp.mark("post-map")
    gp.expect_shell_tree()
    gp.mark("root-ok")
    gp.mark("child-count-ok")
    gp.mark("columns-ok")
    gp.mark("catalog-ok")
    gp.mark("data-ready")
    gp.select_view_tab(0)
    gp.pump(ms=100)
    edit_m0(gp)
    layers_m1(gp)
    navigate(gp)
    gp.select_view_tab(1)
    gp.pump(ms=400)
    gp.expect_scene_visible()
    gp.wait_viewport(face="scene", ms=90000, frame=1)
    gp.mark("scene-frame-ok")
    gp.mark("scene-ready")
    gp.mark("pre-orbit")
    gp.activate_tool(id="view3d.trackball")
    gp.expect_tool(id="view3d.trackball", fail_rc=23)
    gp.mark("orbit-activated")
    gp.camera_fly(mode="orbit", ms=600, steps=8)
    gp.mark("orbit-dispatched")
    gp.expect_orbit_moved()
    gp.mark("orbit-ok")
    gp.select_view_tab(0)
    gp.pump(ms=100)
    present(gp)
    gp.mark("layout-checked")
    gp.expect_layout_bounds()
    gp.expect_map_hwnd_sync()
    gp.mark("map-bounds-ok")
    gp.mark("hwnd-sync-ok")
    milestones(gp)
