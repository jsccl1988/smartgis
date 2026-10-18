# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Digitize point / line / polygon, then a small GeoJSON round-trip.
# Status-bar text and FeatureInfo stay UI; geom and tool ids are the gate.

import os
import tempfile

from steps.edit.digitize import append_line, append_point, append_polygon
from steps.layer.mutate import add_layer
from steps.probe import require_count, require_grew
from steps.session.document import close_document, open_path, save_document


def edit_m0(sg):
    sg.require_edit_tool_session()
    append_point(
        sg,
        tool_fail_rc=12,
        expect_fail_rc=13,
        after_tool=lambda: sg.mark("edit-point"),
    )
    sg.mark("input-point-ok")
    sg.tool(id="selection.point", fail_rc=17)
    sg.expect_tool(id="select.point", fail_rc=18)
    sg.tool(id="selection.clear", fail_rc=19)
    sg.mark("selection-ok")

    append_line(sg, 60)
    sg.mark("input-line-ok")
    sg.mark("m0-line-ok")

    # Drop the seeded china pack so write_document stays inside the smoke budget.
    close_document(sg)
    add_layer(sg, "m0_roundtrip", "LineString")
    append_line(sg, 62)
    out = os.path.join(
        tempfile.gettempdir(), "smartgis_m0_%s.geojson" % os.getpid()
    )
    save_document(sg, out)
    close_document(sg)
    open_path(sg, out, fail_rc=63)
    require_count(sg, 1, "m0 round-trip produced no features")
    try:
        os.remove(out)
    except OSError:
        pass
    sg.mark("m0-save-ok")

    before = int(sg.feature_count())
    append_polygon(sg, 64)
    require_grew(sg, before, "polygon append did not grow the document")
    sg.mark("input-poly-ok")
    sg.mark("input-ok")

    sg.tool(id="view.backend.maplibre", fail_rc=43)
    sg.tool(id="view.backend.rhi", fail_rc=45)
    sg.mark("backend-ok")


def main():
    import glang.plugin as gp

    edit_m0(gp)
