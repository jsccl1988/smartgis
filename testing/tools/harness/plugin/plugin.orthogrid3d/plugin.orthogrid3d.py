# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Orthogrid3D hex volume integration.
# IR: horizon HWND → view scene → plugin orthogrid3d.create_hex_grid.

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
    vts = gp.sidecar_path(rel="..\\data\\plugin\\orthogrid3d.vts")
    if not vts:
        gp.mark("vts-path-fail")
        raise RuntimeError("vts-path-fail")
    gp.run_processing(id="orthogrid3d.create_hex_grid", args='{"nx":16,"ny":14,"nz":8,"vts_path":"%s"}' % _json_slot(vts))
    gp.mark("orthogrid3d-ok")
    gp.fit_scene_box()
    gp.pump(ms=200)
    gp.mark("fit-box-ok")
    gp.camera_fly(mode="orbit", ms=1800, steps=22)
    gp.mark("camera-fly-ok")
    gp.export_bmp(leaf="plugin-showcase-orthogrid3d.bmp", frame="scene3d")
    gp.mark("bmp-ok")
    gp.mark("pass")
