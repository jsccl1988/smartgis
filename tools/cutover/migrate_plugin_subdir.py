# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""One-shot L1 plugin subdirectory move + include rewrite (I1, no shims)."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLUGIN = ROOT / "src" / "plugin"


def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd))
    subprocess.check_call(cmd, cwd=ROOT)


def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def git_mv(src: Path, dst: Path) -> None:
    ensure_dir(dst.parent)
    if not src.exists():
        print("skip missing", src)
        return
    if dst.exists():
        print("skip exists", dst)
        return
    run(["git", "mv", str(src.relative_to(ROOT)).replace("\\", "/"),
         str(dst.relative_to(ROOT)).replace("\\", "/")])


def move_many(pairs: list[tuple[str, str]]) -> None:
    for a, b in pairs:
        git_mv(PLUGIN / a, PLUGIN / b)


def main() -> int:
    ensure_dir(PLUGIN / "host")
    ensure_dir(PLUGIN / "legacy")
    for d in ("legacy_dem", "legacy_proj", "legacy_print", "legacy_model3d",
              "legacy_orthogrid"):
        ensure_dir(PLUGIN / d)

    # --- host ---
    host_files = [
        "host_test.cc",
        "legacy_am.cc", "legacy_am.h",
        "legacy_cmd.cc", "legacy_cmd.h",
        "manager_view.cc", "manager_view.h",
        "manifest.cc", "manifest.h",
        "official_key.h",
        "processing.cc", "processing.h",
        "registry.cc", "registry.h",
        "signature.cc", "signature.h", "signature_test_key.h",
        "store.cc", "store.h",
    ]
    move_many([(f, f"host/{f}") for f in host_files])

    # --- legacy aux DLL ---
    legacy_files = [
        "module.cpp", "module.h",
        "module_manager.cpp", "module_manager.h",
        "plugin_msg.cpp", "plugin_msg.h",
    ]
    move_many([(f, f"legacy/{f}") for f in legacy_files])

    # --- legacy_dem ---
    dem_legacy = [
        "demcreater_plug.cpp", "demcreater_plug.h",
        "dem_creater.cpp", "dem_creater.h", "dem_creater.rc",
        "dlg_about.cpp", "dlg_about.h",
        "dlg_grid_loader.cpp", "dlg_grid_loader.h",
        "dlg_tin_loader.cpp", "dlg_tin_loader.h",
        "ReadMe.txt", "resource.h",
        "SmtAMDemCreater.aps", "SmtAMDemCreater.def",
        "stdafx.cpp", "stdafx.h", "targetver.h",
    ]
    move_many([(f"dem/{f}", f"legacy_dem/{f}") for f in dem_legacy])
    if (PLUGIN / "dem" / "res").exists():
        git_mv(PLUGIN / "dem" / "res", PLUGIN / "legacy_dem" / "res")

    # --- legacy_proj ---
    proj_legacy = [
        "dlg_map_prj.cpp", "dlg_map_prj.h",
        "dlg_map_prj_do_grid.cpp", "dlg_map_prj_do_grid.h",
        "dlg_map_prj_do_xy.cpp", "dlg_map_prj_do_xy.h",
        "mapprj_plug.cpp", "mapprj_plug.h",
        "map_project.cpp", "map_project.h", "map_project.rc",
        "ReadMe.txt", "resource.h",
        "SmtAMMapProject.aps",
        "stdafx.cpp", "stdafx.h", "targetver.h",
    ]
    move_many([(f"proj/{f}", f"legacy_proj/{f}") for f in proj_legacy])
    if (PLUGIN / "proj" / "res").exists():
        git_mv(PLUGIN / "proj" / "res", PLUGIN / "legacy_proj" / "res")

    # --- legacy_print ---
    print_legacy = [
        "dlg_2d_xview.cpp", "dlg_2d_xview.h",
        "mapprint_plugin.cpp", "mapprint_plugin.h",
        "map_print.cpp", "map_print.h", "map_print.rc",
        "ReadMe.txt", "resource.h",
        "SmtAMMapPrint.aps",
        "stdafx.cpp", "stdafx.h", "targetver.h",
    ]
    move_many([(f"print/{f}", f"legacy_print/{f}") for f in print_legacy])
    if (PLUGIN / "print" / "res").exists():
        git_mv(PLUGIN / "print" / "res", PLUGIN / "legacy_print" / "res")

    # --- legacy_model3d ---
    m3d_legacy = [
        "model3d_creater_plugin.cpp", "model3d_creater_plugin.h",
        "model_3d_creater.cpp", "model_3d_creater.h", "model_3d_creater.rc",
        "ReadMe.txt", "resource.h",
        "stdafx.cpp", "stdafx.h", "targetver.h",
    ]
    move_many([(f"model3d/{f}", f"legacy_model3d/{f}") for f in m3d_legacy])
    if (PLUGIN / "model3d" / "res").exists():
        git_mv(PLUGIN / "model3d" / "res", PLUGIN / "legacy_model3d" / "res")

    # --- legacy_orthogrid ---
    og_legacy = [
        "baogcreater_plug.h", "baogrid_creater.h", "baorthgrid.h",
        "creater.cpp", "creater.h", "creater.rc",
        "orthogrid_bas_struct.h",
        "plug.cpp", "plug.h",
        "ReadMe.txt", "resource.h",
        "stdafx.cpp", "stdafx.h", "targetver.h",
    ]
    move_many([(f"orthogrid/{f}", f"legacy_orthogrid/{f}") for f in og_legacy])
    if (PLUGIN / "orthogrid" / "res").exists():
        git_mv(PLUGIN / "orthogrid" / "res", PLUGIN / "legacy_orthogrid" / "res")

    # --- include rewrite map (longest keys first) ---
    repl: list[tuple[str, str]] = [
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
        # domain leftovers
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
        ('"plugin/legacy/orthogrid/resource.h"',
         '"plugin/legacy/orthogrid/resource.h"'),
        ('"plugin/legacy/orthogrid/targetver.h"',
         '"plugin/legacy/orthogrid/targetver.h"'),
        ('"plugin/legacy/orthogrid/stdafx.h"', '"plugin/legacy/orthogrid/stdafx.h"'),
    ]
    # Sort by old key length descending so longer paths win.
    repl.sort(key=lambda x: len(x[0]), reverse=True)

    exts = {".h", ".hh", ".hpp", ".cc", ".cpp", ".cxx", ".c", ".mm", ".md", ".gn",
            ".gni", ".py"}
    skip_dirs = {".git", "out", "third_party", "node_modules"}

    changed = 0
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        for name in filenames:
            path = Path(dirpath) / name
            if path.suffix.lower() not in exts and name not in ("BUILD.gn",):
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                try:
                    text = path.read_text(encoding="gbk")
                except OSError:
                    continue
            orig = text
            for old, new in repl:
                text = text.replace(old, new)
            if text != orig:
                path.write_text(text, encoding="utf-8", newline="\n")
                changed += 1
                print("rewrite", path.relative_to(ROOT))

    print(f"rewrote {changed} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
