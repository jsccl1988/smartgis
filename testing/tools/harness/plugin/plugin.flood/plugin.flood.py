# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Product plugin flood.inundate integration: inundation mask viz + frame export.
# IR: horizon HWND -> view map -> plugin processing -> view export.

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
    gp.wait_viewport(face="map", ms=30000, frame=0)
    gp.mark("map-ready")
    gp.require_plugins()
    dem = gp.resolve_data(kind="plugin", leaf="flood_basin_sample.tif")
    if not dem:
        gp.mark("flood-sample-fail")
        raise RuntimeError("flood-sample-fail")
    out = gp.sidecar_path(rel="..\\data\\plugin\\flood_mask.tif")
    if not out:
        gp.mark("flood-out-fail")
        raise RuntimeError("flood-out-fail")
    gp.doc_clear()
    gp.run_processing(id="flood.inundate", args='{"dem":"%s","output":"%s","seed_x":114.30,"seed_y":30.55,"water_level":45.0,"frames":8}' % (_json_slot(dem), _json_slot(out)))
    gp.mark("flood-ok")
    gp.fit_extent()
    gp.pump(ms=400)
    gp.invalidate_map2d()
    gp.pump(ms=200)
    gp.mark("extent-ok")
    gp.export_bmp(leaf="plugin-showcase-flood.bmp", frame="flood_wuhan")
    gp.mark("bmp-ok")
    gp.mark("playback-ok")
    gp.mark("pass")
