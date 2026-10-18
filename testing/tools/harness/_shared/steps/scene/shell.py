# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Shared scene-tab shell for world3d suite scripts.
# GPU present stays inside export_bmp / camera_fly.

from steps.session.shell import attach_plugins


def open_scene(sg):
    sg.mark("show")
    sg.pump(ms=400)
    sg.require_hwnd()
    sg.mark("hwnd-ok")
    sg.suppress_dialogs(on=1)
    sg.select_map_tab(1)
    sg.pump(ms=400)
    sg.wait_viewport(face="scene", ms=90000, frame=0)
    sg.mark("scene-ready")
    attach_plugins(sg)


def finish_orbit(sg, mode, ms, steps):
    sg.fit_scene_box()
    sg.pump(ms=200)
    sg.mark("fit-box-ok")
    sg.camera_fly(mode=mode, ms=ms, steps=steps)
    sg.mark("camera-fly-ok")
    sg.mark("pass")
