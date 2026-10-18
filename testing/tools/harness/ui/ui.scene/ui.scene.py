# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Scene3D tab chrome BMP (suite "ui.scene").

import glang.plugin as gp

def main():
    gp.mark("scene")
    gp.apply_ui_theme()
    gp.mark("theme-ok")
    gp.mark("pump-pre")
    gp.pump(600)
    gp.mark("pump-post")
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.mark("interact-pre")
    gp.apply_scenario_panels("scene")
    gp.mark("interact-post")
    gp.layout_gate("scene")
    gp.ui_present_capture("scene")
