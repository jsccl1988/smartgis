# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Cross-plugin campaign. Fail closed: a raised step skips every later mark,
# including campaign-pass.
#
# Order: China seed, one appended point, native.buffer, native.clip,
# print style if the file is present, BMP export, then one scene orbit.
#
# Launch is HARNESS_SUITE=browser.campaign (loop_runner sets that from the
# suite id). suite.json argv keeps the retired --harness flag, the closest
# existing switch: GIS steps plus a scene orbit. The Views launcher strips
# it and does not select the script from argv.

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
    from steps.capture.print_page import print_style_path
    from steps.edit.digitize import append_point
    from steps.edit.process import buffer_and_clip

    gp.pump(ms=400)
    gp.require_hwnd()
    gp.suppress_dialogs(on=1)
    gp.wait_viewport(face="map", ms=90000, frame=0)
    gp.require_plugins()
    gp.select_map_tab(0)
    gp.pump(ms=200)

    gp.run_processing(id="map2d.seed", args='{"mode":"china"}')
    if int(gp.feature_count()) < 3:
        raise RuntimeError("campaign china seed")
    gp.mark("campaign-china-ok")

    gp.require_edit_tool_session()
    before = int(gp.feature_count())
    append_point(gp)
    if int(gp.feature_count()) <= before:
        raise RuntimeError("campaign digitize did not append")
    gp.mark("campaign-digitize-ok")

    buffer_and_clip(
        gp,
        "campaign-buffer-ok",
        "campaign-clip-ok",
        stem="smartgis_campaign",
    )

    style = print_style_path()
    if os.path.isfile(style):
        gp.apply_style_file(path=style)
    gp.export_bmp(leaf="browser-campaign.bmp", frame="document_extent")
    gp.mark("campaign-export-ok")

    gp.select_view_tab(1)
    gp.pump(ms=400)
    gp.expect_scene_visible()
    gp.wait_viewport(face="scene", ms=90000, frame=1)
    gp.activate_tool(id="view3d.trackball")
    gp.expect_tool(id="view3d.trackball", fail_rc=23)
    gp.camera_fly(mode="orbit", ms=600, steps=8)
    gp.expect_orbit_moved()
    gp.mark("campaign-orbit-ok")
    gp.mark("campaign-pass")
