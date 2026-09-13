#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Rewrite pragma comment(lib,\"Smt*.lib\") and Smt_* namespaces."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Old stem -> new stem (matches dll_stem map)
STEMS = {
    "SmtCore": "core",
    "SmtBaseLib": "style",
    "SmtSysCore": "sys",
    "SmtGeoCore": "geo",
    "Smt3DGeoCore": "geo",
    "SmtGisCore": "gis",
    "SmtGisPrj": "proj",
    "SmtTinMesh": "tin",
    "SmtStaCore": "stat",
    "SmtNetCore": "net",
    "SmtRender": "render",
    "SmtRenderer": "render",
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
    "SmtAppCore": "app_core",
}

# Longer namespace identifiers first.
NAMESPACES = [
    ("Smt_SDEDevMgr", "sdb"),
    ("Smt_GroupTool", "tool"),
    ("Smt_3DBase", "render"),
    ("Smt_3Drd", "render"),
    ("Smt_Core", "base"),
    ("Smt_Base", "base"),
    ("Smt_Sys", "sys"),
    ("Smt_GIS", "sdb"),
    ("Smt_Geo", "geo"),
    ("Smt_Rd", "render"),
    ("Smt_Net", "net"),
    ("Smt_UI", "ui"),
    ("Smt_AM", "plugin"),
    ("Smt_Tool", "tool"),
    ("Smt_Sta", "stat"),
]

SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".inc"}


def rewrite_pragma(text: str) -> tuple[str, int]:
    n = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal n
        name = m.group(1)
        debug = name.endswith("D")
        stem = name[:-1] if debug and name[:-1] in STEMS else name
        if stem not in STEMS:
            # try without assuming D only when mapped
            if name in STEMS:
                new = STEMS[name] + ".lib"
                n += 1
                return m.group(0).replace(name + ".lib", new)
            return m.group(0)
        new_stem = STEMS[stem] + ("D" if debug else "")
        n += 1
        return m.group(0).replace(name + ".lib", new_stem + ".lib")

    return re.sub(r'pragma\s+comment\s*\(\s*lib\s*,\s*"([A-Za-z0-9_]+)\.lib"', repl, text), n


def rewrite_ns(text: str) -> tuple[str, int]:
    n = 0
    for old, new in NAMESPACES:
        # namespace X / using namespace X
        for pat in (
            rf"\bnamespace\s+{re.escape(old)}\b",
            rf"\busing\s+namespace\s+{re.escape(old)}\b",
            rf"\b{re.escape(old)}\s*::",
        ):

            def make_repl(o: str, nw: str, p: str):
                def r(m: re.Match[str]) -> str:
                    nonlocal n
                    n += 1
                    s = m.group(0)
                    return s.replace(o, nw)

                return r

            text, c = re.subn(pat, make_repl(old, new, pat), text)
            # re.subn with function - count via side effect; c is match count
            # actually we're double counting if we use both - use c from subn
        # simpler sequential replace with word boundaries for :: and namespace forms
    # redo cleanly
    n = 0
    for old, new in NAMESPACES:
        patterns = [
            (re.compile(rf"\bnamespace\s+{re.escape(old)}\b"), f"namespace {new}"),
            (re.compile(rf"\busing\s+namespace\s+{re.escape(old)}\b"), f"using namespace {new}"),
            (re.compile(rf"\b{re.escape(old)}\s*::"), f"{new}::"),
        ]
        for cre, repl in patterns:
            text, c = cre.subn(repl, text)
            n += c
    return text, n


def main() -> None:
    files = pragma_n = ns_n = 0
    for root in (ROOT / "src", ROOT / "testing"):
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            t1, a = rewrite_pragma(text)
            t2, b = rewrite_ns(t1)
            if a or b:
                p.write_text(t2, encoding="utf-8", newline="\n")
                files += 1
                pragma_n += a
                ns_n += b
    print(f"files={files} pragma={pragma_n} namespaces={ns_n}")


if __name__ == "__main__":
    main()
