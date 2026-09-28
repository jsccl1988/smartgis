# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""L3 orchestration sample: pick files and call L1 processing ids."""

import json

import smartgis


PLUGIN_ID = "smartgis.sample_product_orchestrate"


def _pick_path(pattern):
    result = smartgis.ui.pick_open_file(pattern)
    if not result or not result.get("accepted"):
        return None
    path = result.get("path") or ""
    if not path:
        return None
    return path


def start(host):
    def _tin_from_xyz(_args):
        path = _pick_path("*.xyz;*.txt;*.*")
        if path is None:
            return True  # cancel
        payload = json.dumps(
            {
                "vertex_path": path,
                "separator": "comma",
                "head_skip": 1,
                "line_skip": 4,
                "col_x": 0,
                "col_y": 1,
                "col_z": 2,
            }
        )
        ok = bool(host.run_processing("dem.tin_from_xyz", payload))
        if not ok:
            smartgis.ui.show_message_box(
                "error",
                "dem.tin_from_xyz failed (check path / map seam / builtin DEM).",
            )
        return ok

    def _create_orth_grid(_args):
        path = _pick_path("*.txt;*.*")
        if path is None:
            return True  # cancel
        payload = json.dumps({"path": path})
        ok = bool(host.run_processing("baogrid.create_orth_grid", payload))
        if not ok:
            smartgis.ui.show_message_box(
                "error",
                "baogrid.create_orth_grid failed (check boundary file / baogrid).",
            )
        return ok

    host.contribute_command(
        PLUGIN_ID,
        "sample.orch.tin_from_xyz",
        "Orchestrate TIN from XYZ",
        "tools",
        _tin_from_xyz,
    )
    host.contribute_command(
        PLUGIN_ID,
        "sample.orch.create_orth_grid",
        "Orchestrate orth grid",
        "tools",
        _create_orth_grid,
    )


def stop():
    pass
