# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Product plugin geochem.analyze integration → graded samples + IDW + BMP.
# IR: horizon HWND → view map → plugin processing → view export.

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
    csv = gp.resolve_data(kind="plugin", leaf="geochem_samples.csv")
    if not csv:
        gp.mark("geochem-sample-fail")
        raise RuntimeError("geochem-sample-fail")
    out = gp.sidecar_path(rel="..\\data\\plugin\\geochem_idw.tif")
    if not out:
        gp.mark("geochem-out-fail")
        raise RuntimeError("geochem-out-fail")
    gp.doc_clear()
    gp.run_processing(id="geochem.analyze", args='{"input":"%s","element":"Cu","correlate":"Au","cells":32,"classes":5,"output":"%s"}' % (_json_slot(csv), _json_slot(out)))
    gp.mark("geochem-ok")
    gp.fit_extent()
    gp.pump(ms=300)
    gp.run_processing(id="geochem.stats", args='{"input":"%s","element":"Cu","correlate":"Au","bins":8}' % _json_slot(csv))
    gp.mark("stats-ok")
    gp.pump(ms=200)
    gp.fit_extent()
    gp.invalidate_map2d()
    gp.pump(ms=200)
    gp.mark("extent-ok")
    gp.export_bmp(leaf="plugin-showcase-geochem.bmp", frame="geochem_samples")
    gp.mark("bmp-ok")
    gp.mark("pass")
