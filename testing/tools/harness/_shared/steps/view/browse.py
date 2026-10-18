# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Map tab seed, then scene orbit / wheel / still.
# Do not select_map_tab again after capture (FlyCube remount clears the frame).

from steps.session.china import open_china_city


def browse(sg):
    sg.pump(ms=300)
    sg.require_hwnd()
    sg.mark("show")
    sg.mark("hwnd-ok")
    sg.mark("suite-browse3d")
    sg.suppress_dialogs(on=1)
    sg.pump(ms=200)
    sg.select_map_tab(0)
    sg.pump(ms=200)
    open_china_city(sg)
    sg.fit_extent()
    sg.pump(ms=200)
    sg.select_map_tab(1)
    sg.pump(ms=400)
    sg.expect_scene_visible()
    sg.wait_viewport(face="scene", ms=60000, frame=0)
    sg.mark("scene-ready")
    sg.fit_scene_box()
    sg.pump(ms=200)
    sg.mark("fit-box-ok")
    sg.camera_fly(mode="orbit", ms=1600, steps=20)
    sg.expect_orbit_moved()
    sg.mark("browse3d-orbit-ok")
    sg.mark("camera-fly-ok")
    sg.wheel("map_client", 120, 120, -120)
    sg.pump(ms=100)
    sg.wheel("map_client", 120, 120, 120)
    sg.mark("browse3d-wheel-ok")
    sg.pump(ms=150)
    sg.capture_browse_still(face="scene3d")
    sg.mark("browse-exit")
    sg.mark("pass")
    sg.stop_map_timers()


def main():
    import glang.plugin as gp

    browse(gp)
