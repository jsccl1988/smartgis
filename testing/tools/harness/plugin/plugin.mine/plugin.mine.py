# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Mine Scene3D integration: borehole CSV → stratum TIN + prism volume.
# IR: horizon HWND → view scene → plugin processing.

import glang.plugin as gp

def _json_slot(value):
    # Paths land inside a JSON string; escape quotes and backslashes.
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main():
    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.select_map_tab(1)
    gp.pump(ms=400)
    gp.wait_viewport(face="scene", ms=90000, frame=0)
    gp.mark("scene-ready")
    gp.require_plugins()
    csv = gp.resolve_data(kind="plugin", leaf="mine_boreholes.csv")
    if not csv:
        gp.mark("mine-sample-fail")
        raise RuntimeError("mine-sample-fail")
    gp.run_processing(id="mine.interpolate_stratum", args='{"input":"%s","stratum_id":"clay"}' % _json_slot(csv))
    gp.mark("mine-ok")
    gp.run_processing(id="mine.prism_volume", args='{"input":"%s","top_stratum_id":"clay","bottom_stratum_id":"sand"}' % _json_slot(csv))
    gp.mark("prism-ok")
    gp.fit_scene_box()
    gp.pump(ms=200)
    gp.mark("fit-box-ok")
    gp.camera_fly(mode="orbit", ms=1800, steps=22)
    gp.mark("camera-fly-ok")
    gp.export_bmp(leaf="plugin-showcase-mine.bmp", frame="scene3d")
    gp.mark("bmp-ok")
    gp.mark("pass")
