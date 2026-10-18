# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Product plugin traffic.cost_path integration → map path viz + frame export.
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
    net = gp.resolve_data(kind="plugin", leaf="traffic_network_sample.geojson")
    if not net:
        gp.mark("traffic-sample-fail")
        raise RuntimeError("traffic-sample-fail")
    out = gp.sidecar_path(rel="..\\data\\plugin\\traffic_path.geojson")
    if not out:
        gp.mark("traffic-out-fail")
        raise RuntimeError("traffic-out-fail")
    gp.doc_clear()
    gp.run_processing(id="traffic.cost_path", args='{"network":"%s","output":"%s","start_x":116.335,"start_y":39.870,"end_x":116.452,"end_y":39.9285,"weight_field":"cost","frames":12}' % (_json_slot(net), _json_slot(out)))
    gp.mark("traffic-ok")
    gp.fit_extent()
    gp.pump(ms=400)
    gp.invalidate_map2d()
    gp.pump(ms=200)
    gp.mark("extent-ok")
    gp.export_bmp(leaf="plugin-showcase-traffic.bmp", frame="traffic_beijing")
    gp.mark("bmp-ok")
    gp.mark("playback-ok")
    gp.mark("pass")
