# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Map2d orthogrid integration: boundary file → baogrid.create_orth_grid.
# IR: horizon HWND → view map → plugin processing.

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
    bnd = gp.resolve_data(kind="plugin", leaf="orthogrid_sample.gridbnd")
    if not bnd:
        gp.mark("bnd-missing")
        raise RuntimeError("bnd-missing")
    gp.run_processing(id="baogrid.create_orth_grid", args='{"path":"%s","elliptic_iters":3}' % _json_slot(bnd))
    gp.mark("orthogrid-ok")
    gp.fit_extent()
    gp.pump(ms=300)
    gp.invalidate_map2d()
    gp.pump(ms=200)
    gp.mark("extent-ok")
    gp.export_bmp(leaf="plugin-showcase-orthogrid.bmp", frame="document_extent")
    gp.mark("bmp-ok")
    gp.mark("pass")
