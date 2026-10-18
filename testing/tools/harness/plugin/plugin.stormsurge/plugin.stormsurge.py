# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Stormsurge Scene3D integration: coast polyline + schematic DEM inundation.
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
    dem = gp.resolve_data(kind="plugin", leaf="stormsurge_dem_sample.tif")
    if not dem:
        gp.mark("stormsurge-sample-fail")
        raise RuntimeError("stormsurge-sample-fail")
    coast = gp.resolve_data(kind="plugin", leaf="stormsurge_coast_sample.geojson")
    if not coast:
        gp.mark("stormsurge-coast-fail")
        raise RuntimeError("stormsurge-coast-fail")
    out = gp.sidecar_path(rel="..\\data\\plugin\\stormsurge_mask.tif")
    if not out:
        gp.mark("stormsurge-out-fail")
        raise RuntimeError("stormsurge-out-fail")
    gp.run_processing(id="stormsurge.load_coast", args='{"coast":"%s"}' % _json_slot(coast))
    gp.mark("coast-ok")
    gp.run_processing(id="stormsurge.run", args='{"dem":"%s","coast":"%s","output":"%s","seed_x":114.30,"seed_y":30.55,"surge_levels":[28,34,40,46,50,54,56,58],"frames":8}' % (_json_slot(dem), _json_slot(coast), _json_slot(out)))
    gp.mark("stormsurge-ok")
    gp.fit_scene_box()
    gp.pump(ms=200)
    gp.mark("fit-box-ok")
    gp.camera_fly(mode="orbit", ms=1800, steps=22)
    gp.mark("camera-fly-ok")
    gp.export_bmp(leaf="plugin-showcase-stormsurge.bmp", frame="scene3d")
    gp.mark("bmp-ok")
    gp.mark("pass")
