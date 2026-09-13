#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Scan for leftover flat product includes and Smt ABI tokens after cutover."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOTS = [ROOT / "src", ROOT / "testing"]
CODE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl", ".inc"}
GN_SUFFIXES = {".gn", ".gni"}

FLAT_INCLUDE = re.compile(r'#\s*include\s+"([^"]+)"')
EXPORT_SMT = re.compile(r"Export_Smt\w+")
DLL_STEM_SMT = re.compile(r'dll_stem\s*=\s*"Smt[^"]*"')

# Third-party / SDK / MFC headers that correctly stay basename-only via
# public_configs or system include paths (not //src product headers).
THIRD_PARTY_FLAT = {
    "httplib.h",
    "ed25519.h",
    "ogr_geometry.h",
    "ogrsf_frmts.h",
    "gdal_priv.h",
    "gdal.h",
    "ogr_api.h",
    "ogr_core.h",
    "ogr_feature.h",
    "ogr_spatialref.h",
    "cpl_conv.h",
    "cpl_error.h",
    "cpl_string.h",
    "geos_c.h",
    "ximage.h",
    "tinystr.h",
    "tinyxml.h",
    "tiny_gltf.h",
    "antlr4-runtime.h",
    "TCHAR.h",
    "tchar.h",
    "afxwin.h",
    "afxcmn.h",
    "afxdlgs.h",
    "afxext.h",
    "afxinet.h",
    "afxmt.h",
    "afxole.h",
    "afxpriv.h",
    "afxres.h",
    "afxcontrolbars.h",
    "afxribbon.h",
    "stdafx.h",
    "StdAfx.h",
    "targetver.h",
    "resource.h",
    "Resource.h",
}


def is_product_flat(name: str) -> bool:
    name = name.strip()
    if "/" in name or "\\" in name:
        return False
    # Allow known third-party basenames.
    if name in THIRD_PARTY_FLAT:
        return False
    # MFC / Windows often use these without path.
    if name.lower().startswith("afx"):
        return False
    return True


def iter_files(roots: list[Path], suffixes: set[str]):
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if p.is_file() and p.suffix.lower() in suffixes:
                yield p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument(
        "--strict-pch",
        action="store_true",
        help="Also flag stdafx.h/targetver.h/resource.h flat includes",
    )
    args = ap.parse_args()

    flat: list[str] = []
    exports: list[str] = []
    stems: list[str] = []

    for p in iter_files(SCAN_ROOTS, CODE_SUFFIXES):
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(ROOT).as_posix()
        for i, line in enumerate(text.splitlines(), 1):
            m = FLAT_INCLUDE.search(line)
            if m:
                name = m.group(1)
                if args.strict_pch or is_product_flat(name):
                    if args.strict_pch and name.strip() in {
                        "stdafx.h",
                        "targetver.h",
                        "resource.h",
                        "Resource.h",
                    }:
                        flat.append(f"{rel}:{i}:{line.strip()}")
                    elif is_product_flat(name):
                        flat.append(f"{rel}:{i}:{line.strip()}")
            if EXPORT_SMT.search(line):
                exports.append(f"{rel}:{i}:{line.strip()}")

    for p in iter_files([ROOT / "src", ROOT / "build"], GN_SUFFIXES):
        text = p.read_text(encoding="utf-8", errors="replace")
        rel = p.relative_to(ROOT).as_posix()
        for i, line in enumerate(text.splitlines(), 1):
            if DLL_STEM_SMT.search(line):
                stems.append(f"{rel}:{i}:{line.strip()}")
            if EXPORT_SMT.search(line):
                exports.append(f"{rel}:{i}:{line.strip()}")

    if not args.quiet:
        print(f"flat_product_includes={len(flat)}")
        print(f"Export_Smt={len(exports)}")
        print(f"dll_stem_Smt={len(stems)}")
        for bucket, name in (
            (flat[:40], "flat"),
            (exports[:30], "export"),
            (stems[:30], "stem"),
        ):
            if bucket:
                print(f"--- sample {name} ---")
                print("\n".join(bucket))

    return 1 if (flat or exports or stems) else 0


if __name__ == "__main__":
    sys.exit(main())
