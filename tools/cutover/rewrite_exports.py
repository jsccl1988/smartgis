#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Rename Export_Smt* tokens and dll_stem values per abi-rename-map."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Old Export_* token -> new export macro name (also used as BUILD define).
EXPORT_MAP = {
    "Export_SmtCore": "CORE_EXPORT",
    "Export_SmtBaseLib": "STYLE_EXPORT",
    "Export_SmtSysCore": "SYS_EXPORT",
    "Export_SmtGeoCore": "GEO_EXPORT",
    "Export_Smt3DGeoCore": "GEO_EXPORT",
    "Export_SmtGisCore": "GIS_EXPORT",
    "Export_SmtGisPrj": "PROJ_EXPORT",
    "Export_SmtTinMesh": "TIN_EXPORT",
    "Export_SmtStaCore": "STAT_EXPORT",
    "Export_SmtNetCore": "NET_EXPORT",
    "Export_SmtRenderer": "RENDER_EXPORT",
    "Export_Smt3DRenderer": "RENDER3D_EXPORT",
    "Export_SmtGdiRenderDevice": "RENDER_GDI_EXPORT",
    "Export_SmtGdiSimpleRenderDevice": "RENDER_GDI_SIMPLE_EXPORT",
    "Export_SmtGLRenderDevice": "RENDER_GL_EXPORT",
    "Export_Smt3DBaseLib": "SCENE3D_EXPORT",
    "Export_Smt3DMdLib": "MODEL3D_EXPORT",
    "Export_Smt3DPointCloud": "POINTCLOUD_EXPORT",
    "Export_Smt3DTerrain": "TERRAIN_EXPORT",
    "Export_SmtSDEDeviceMgr": "SDE_MGR_EXPORT",
    "Export_SmtSDEGdalDevice": "SDE_GDAL_EXPORT",
    "Export_SmtSDEMemDevice": "SDE_MEM_EXPORT",
    "Export_SmtSDESmfDevice": "SDE_SMF_EXPORT",
    "Export_SmtSDEWSDevice": "SDE_WS_EXPORT",
    "Export_SmtToolCore": "TOOL_EXPORT",
    "Export_SmtGroupToolCore": "TOOL_GROUP_EXPORT",
    "Export_SmtGuiCore": "GUI_EXPORT",
    "Export_SmtMFCExCore": "MFC_EX_EXPORT",
    "Export_SmtXViewCore": "XVIEW_EXPORT",
    "Export_SmtXCatalogCore": "XCATALOG_EXPORT",
    "Export_SmtXAMBoxCore": "XAMBOX_EXPORT",
    "Export_SmtStaDiagram": "STAT_CHART_EXPORT",
    "Export_SmtAuxModule": "PLUGIN_EXPORT",
    "Export_SmtAppCore": "APP_CORE_EXPORT",
}

DLL_STEM_MAP = {
    "SmtCore": "core",
    "SmtBaseLib": "style",
    "SmtSysCore": "sys",
    "SmtGeoCore": "geo",
    "SmtGisCore": "gis",
    "SmtGisPrj": "proj",
    "SmtTinMesh": "tin",
    "SmtStaCore": "stat",
    "SmtNetCore": "net",
    "SmtRender": "render",
    "Smt3DRenderer": "render3d",
    "SmtGdiRenderDevice": "render_gdi",
    "SmtGdiSimpleRenderDevice": "render_gdi_simple",
    "SmtGLRenderDevice": "render_gl",
    "Smt3DBaseLib": "scene3d",
    "Smt3DMdLib": "model3d",
    "Smt3DPointCloud": "pointcloud",
    "Smt3DTerrain": "terrain",
    "SmtSDEDeviceMgr": "sde_mgr",
    "SmtSDEGdalDevice": "sde_gdal",
    "SmtSDEMemDevice": "sde_mem",
    "SmtSDESmfDevice": "sde_smf",
    "SmtSDEWSDevice": "sde_ws",
    "SmtToolCore": "tool",
    "SmtGroupToolCore": "tool_group",
    "SmtGuiCore": "gui",
    "SmtMFCExCore": "mfc_ex",
    "SmtXViewCore": "xview",
    "SmtXCatalogCore": "xcatalog",
    "SmtXAMBoxCore": "xambox",
    "SmtStaDiagram": "stat_chart",
    "SmtAuxModule": "plugin",
    "SmtAMDemCreater": "plugin_dem",
    "SmtAMOrthogrid": "plugin_orthogrid",
    "SmtAMBAOGridCreater": "plugin_orthogrid",
    "SmtAM3DModelCreater": "plugin_model3d",
    "SmtAMMapPrint": "plugin_print",
    "SmtAMMapProject": "plugin_proj",
    "SmtAppCore": "app_core",
}

PLUGIN_STEM_STRINGS = {
    "SmtAMDemCreater": "plugin_dem",
    "SmtAMMapProject": "plugin_proj",
    "SmtAMMapPrint": "plugin_print",
    "SmtAM3DModelCreater": "plugin_model3d",
    "SmtAMOrthogrid": "plugin_orthogrid",
    "SmtAMBAOGridCreater": "plugin_orthogrid",
}

SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".inc", ".gn", ".gni", ".md", ".txt"}


def replace_exports(text: str) -> tuple[str, int]:
    n = 0
    # Longer keys first
    for old in sorted(EXPORT_MAP.keys(), key=len, reverse=True):
        new = EXPORT_MAP[old]
        c = text.count(old)
        if c:
            text = text.replace(old, new)
            n += c
    return text, n


def replace_dll_stems(text: str) -> tuple[str, int]:
    n = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal n
        old = m.group(1)
        new = DLL_STEM_MAP.get(old, old)
        if new != old:
            n += 1
        return f'dll_stem = "{new}"'

    return re.sub(r'dll_stem\s*=\s*"(Smt[^"]*)"', repl, text), n


def replace_plugin_strings(text: str) -> tuple[str, int]:
    n = 0
    for old, new in PLUGIN_STEM_STRINGS.items():
        # quoted stems only
        for q in ('"' + old + '"', "'" + old + "'"):
            if q in text:
                text = text.replace(q, q[0] + new + q[0])
                n += 1
    return text, n


def main() -> int:
    exp_n = stem_n = plug_n = 0
    files = 0
    roots = [ROOT / "src", ROOT / "testing", ROOT / "build", ROOT / "docs"]
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            # skip huge / generated
            if "third_party" in p.parts:
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            t1, a = replace_exports(text)
            t2, b = replace_dll_stems(t1)
            t3, c = replace_plugin_strings(t2)
            if a or b or c:
                p.write_text(t3, encoding="utf-8", newline="\n")
                exp_n += a
                stem_n += b
                plug_n += c
                files += 1
    print(f"files={files} export_renames={exp_n} dll_stem={stem_n} plugin_strings={plug_n}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
