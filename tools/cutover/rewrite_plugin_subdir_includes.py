# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Rewrite plugin includes after subdirectory move (encoding-safe)."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

REPL = [
    ('"plugin/host/legacy_am.h"', '"plugin/host/legacy_am.h"'),
    ('"plugin/host/legacy_cmd.h"', '"plugin/host/legacy_cmd.h"'),
    ('"plugin/host/manager_view.h"', '"plugin/host/manager_view.h"'),
    ('"plugin/host/manifest.h"', '"plugin/host/manifest.h"'),
    ('"plugin/host/official_key.h"', '"plugin/host/official_key.h"'),
    ('"plugin/host/processing.h"', '"plugin/host/processing.h"'),
    ('"plugin/host/registry.h"', '"plugin/host/registry.h"'),
    ('"plugin/host/signature_test_key.h"', '"plugin/host/signature_test_key.h"'),
    ('"plugin/host/signature.h"', '"plugin/host/signature.h"'),
    ('"plugin/host/store.h"', '"plugin/host/store.h"'),
    ('"plugin/legacy/module_manager.h"', '"plugin/legacy/module_manager.h"'),
    ('"plugin/legacy/module.h"', '"plugin/legacy/module.h"'),
    ('"plugin/legacy/plugin_msg.h"', '"plugin/legacy/plugin_msg.h"'),
    ('"plugin/legacy/dem/demcreater_plug.h"', '"plugin/legacy/dem/demcreater_plug.h"'),
    ('"plugin/legacy/dem/dem_creater.h"', '"plugin/legacy/dem/dem_creater.h"'),
    ('"plugin/legacy/dem/dlg_about.h"', '"plugin/legacy/dem/dlg_about.h"'),
    ('"plugin/legacy/dem/dlg_grid_loader.h"', '"plugin/legacy/dem/dlg_grid_loader.h"'),
    ('"plugin/legacy/dem/dlg_tin_loader.h"', '"plugin/legacy/dem/dlg_tin_loader.h"'),
    ('"plugin/legacy/dem/resource.h"', '"plugin/legacy/dem/resource.h"'),
    ('"plugin/legacy/dem/targetver.h"', '"plugin/legacy/dem/targetver.h"'),
    ('"plugin/legacy/dem/stdafx.h"', '"plugin/legacy/dem/stdafx.h"'),
    ('"plugin/legacy/proj/dlg_map_prj_do_grid.h"',
     '"plugin/legacy/proj/dlg_map_prj_do_grid.h"'),
    ('"plugin/legacy/proj/dlg_map_prj_do_xy.h"',
     '"plugin/legacy/proj/dlg_map_prj_do_xy.h"'),
    ('"plugin/legacy/proj/dlg_map_prj.h"', '"plugin/legacy/proj/dlg_map_prj.h"'),
    ('"plugin/legacy/proj/mapprj_plug.h"', '"plugin/legacy/proj/mapprj_plug.h"'),
    ('"plugin/legacy/proj/map_project.h"', '"plugin/legacy/proj/map_project.h"'),
    ('"plugin/legacy/proj/resource.h"', '"plugin/legacy/proj/resource.h"'),
    ('"plugin/legacy/proj/targetver.h"', '"plugin/legacy/proj/targetver.h"'),
    ('"plugin/legacy/proj/stdafx.h"', '"plugin/legacy/proj/stdafx.h"'),
    ('"plugin/legacy/print/dlg_2d_xview.h"', '"plugin/legacy/print/dlg_2d_xview.h"'),
    ('"plugin/legacy/print/mapprint_plugin.h"',
     '"plugin/legacy/print/mapprint_plugin.h"'),
    ('"plugin/legacy/print/map_print.h"', '"plugin/legacy/print/map_print.h"'),
    ('"plugin/legacy/print/resource.h"', '"plugin/legacy/print/resource.h"'),
    ('"plugin/legacy/print/targetver.h"', '"plugin/legacy/print/targetver.h"'),
    ('"plugin/legacy/print/stdafx.h"', '"plugin/legacy/print/stdafx.h"'),
    ('"plugin/legacy/model3d/model3d_creater_plugin.h"',
     '"plugin/legacy/model3d/model3d_creater_plugin.h"'),
    ('"plugin/legacy/model3d/model_3d_creater.h"',
     '"plugin/legacy/model3d/model_3d_creater.h"'),
    ('"plugin/legacy/model3d/resource.h"', '"plugin/legacy/model3d/resource.h"'),
    ('"plugin/legacy/model3d/targetver.h"', '"plugin/legacy/model3d/targetver.h"'),
    ('"plugin/legacy/model3d/stdafx.h"', '"plugin/legacy/model3d/stdafx.h"'),
    ('"plugin/legacy/orthogrid/baogcreater_plug.h"',
     '"plugin/legacy/orthogrid/baogcreater_plug.h"'),
    ('"plugin/legacy/orthogrid/baogrid_creater.h"',
     '"plugin/legacy/orthogrid/baogrid_creater.h"'),
    ('"plugin/legacy/orthogrid/baorthgrid.h"',
     '"plugin/legacy/orthogrid/baorthgrid.h"'),
    ('"plugin/legacy/orthogrid/creater.h"', '"plugin/legacy/orthogrid/creater.h"'),
    ('"plugin/legacy/orthogrid/orthogrid_bas_struct.h"',
     '"plugin/legacy/orthogrid/orthogrid_bas_struct.h"'),
    ('"plugin/legacy/orthogrid/plug.h"', '"plugin/legacy/orthogrid/plug.h"'),
    ('"plugin/legacy/orthogrid/resource.h"', '"plugin/legacy/orthogrid/resource.h"'),
    ('"plugin/legacy/orthogrid/targetver.h"',
     '"plugin/legacy/orthogrid/targetver.h"'),
    ('"plugin/legacy/orthogrid/stdafx.h"', '"plugin/legacy/orthogrid/stdafx.h"'),
]
REPL = sorted(REPL, key=lambda x: len(x[0]), reverse=True)

EXTS = {".h", ".hh", ".hpp", ".cc", ".cpp", ".cxx", ".c", ".md", ".gn", ".gni",
        ".py"}
SKIP = {".git", "out", "third_party", "node_modules"}


def main() -> int:
    n = 0
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        if any(part in SKIP for part in path.parts):
            continue
        if path.suffix.lower() not in EXTS and path.name != "BUILD.gn":
            continue
        raw = path.read_bytes()
        text = raw.decode("utf-8", errors="surrogateescape")
        orig = text
        for old, new in REPL:
            text = text.replace(old, new)
        if text != orig:
            path.write_bytes(text.encode("utf-8", errors="surrogateescape"))
            n += 1
            print(path.relative_to(ROOT))
    print(f"rewrote {n} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
