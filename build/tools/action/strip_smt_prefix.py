#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Strip SMT_/Smt/smt_ prefixes across product trees (option 4).

Skips src/legacy/, branches/, third_party/, out/.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

SKIP_DIR_PARTS = {
    "legacy",
    "branches",
    "third_party",
    "out",
    ".git",
    "node_modules",
    ".src",
    "archive",  # historical docs keep old names
}

TEXT_EXTS = {
    ".h",
    ".hh",
    ".hpp",
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".inl",
    ".md",
    ".mdc",
    ".gn",
    ".gni",
    ".py",
    ".json",
    ".bat",
    ".ps1",
    ".yml",
    ".yaml",
    ".txt",
    ".html",
    ".css",
    ".il",
}

# Longest-first explicit type / symbol renames (product-owned).
TYPE_MAP: list[tuple[str, str]] = [
    ("SmtRhi2dClearBgraSubmitFn", "Rhi2dClearBgraSubmitFn"),
    ("SmtRhi2dSetBgraSubmitFn", "Rhi2dSetBgraSubmitFn"),
    ("SmtRhi2dBgraSubmitFn", "Rhi2dBgraSubmitFn"),
    ("SmtRhi2dClearBgraSubmit", "Rhi2dClearBgraSubmit"),
    ("SmtRhi2dSetBgraSubmit", "Rhi2dSetBgraSubmit"),
    ("SmtDeferChinaBrowser", "DeferChinaBrowser"),
    ("SmtPlaybackBrowser", "PlaybackBrowser"),
    ("SmtFlashBrowser", "FlashBrowser"),
    ("SmtMenuBrowser", "MenuBrowser"),
    ("SmtBlitBrowser", "BlitBrowser"),
    ("SmtRenderDevice", "RenderDevice2d"),
    ("Smt3DPointCloud", "PointCloud3d"),
    ("SmtSDEGdalDevice", "GdalDevice"),
    ("SmtCalculator", "Calculator"),
    ("SmtMaterial", "Material"),
    ("SmtTerrain", "Terrain"),
    ("SmtScene", "Scene"),
    ("SmtStyle", "Style"),
    ("SmtTimer", "Timer"),
    ("SmtMap", "Map"),
    ("SmtGL", "GL"),
    ("SmtApp", "App"),
]

# File / template renames (path + text).
FILE_RENAMES: list[tuple[str, str]] = [
    ("build/smt_compat.h", "build/win_compat.h"),
]

# Longest-first identifier renames (word boundary).
IDENT_MAP: list[tuple[str, str]] = [
    # C export ABI (product scenic)
    ("smt_stereo_hwnd_capture_bgr24", "stereo_hwnd_capture_bgr24"),
    ("smt_stereo_hwnd_present", "stereo_hwnd_present"),
    ("smt_stereo_hwnd_destroy", "stereo_hwnd_destroy"),
    ("smt_stereo_hwnd_create", "stereo_hwnd_create"),
    ("smt_stereo_hwnd_resize", "stereo_hwnd_resize"),
    ("smt_stereo_hwnd_blit", "stereo_hwnd_blit"),
    ("smt_scene_world_mirror", "scene_world_mirror"),
    ("smt_leftover_session", "leftover_session"),
    ("smt_enable_maplibre", "enable_maplibre"),
    ("smt_report_browser_test", "report_browser_test"),
    ("smt_store_", "store_"),
    # GN template / args
    ("smt_shared_library", "product_shared_library"),
    ("smt_has_flycube_src", "has_flycube_src"),
    ("smt_has_rapidjson", "has_rapidjson"),
    ("smt_has_protobuf", "has_protobuf"),
    ("smt_has_tinygltf", "has_tinygltf"),
    ("smt_has_pugixml", "has_pugixml"),
    ("smt_has_flycube", "has_flycube"),
    ("smt_has_python", "has_python"),
    ("smt_has_assimp", "has_assimp"),
    ("smt_has_skia", "has_skia"),
    ("smt_has_cuda", "has_cuda"),
    ("smt_has_pdal", "has_pdal"),
    ("smt_python_enabled", "python_enabled"),
    ("smt_python_include", "python_include"),
    ("smt_python_prefix", "python_prefix"),
    ("smt_python_libdir", "python_libdir"),
    ("smt_python_root", "python_root"),
    ("smt_python_dll", "python_dll"),
    ("smt_build_render", "build_render"),
    ("smt_build_views", "build_views"),
    ("smt_build_app", "build_app"),
    ("smt_cuda_home", "cuda_home"),
    ("smt_compat.h", "win_compat.h"),
    ("smt_compat", "win_compat"),
    ("smt_vs", "win_vs"),
]


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    parts = path.relative_to(ROOT).parts
    if rel.startswith("src/legacy/") or "/legacy/" in rel and parts[:2] == (
        "src",
        "legacy",
    ):
        return True
    if parts and parts[0] in {"branches", "third_party", "out", ".git"}:
        return True
    if "archive" in parts:
        return True
    if path.name in {"strip_smt_prefix.py", "scenic_strip_smt.py"}:
        return True
    return False


def rewrite_text(text: str) -> str:
    for old, new in TYPE_MAP:
        text = re.sub(rf"\b{re.escape(old)}\b", new, text)
    for old, new in IDENT_MAP:
        text = re.sub(rf"\b{re.escape(old)}\b", new, text)

    # Compile defines / export macros SMT_HAS_* → HAS_*
    text = re.sub(r"\bSMT_HAS_", "HAS_", text)
    # Export / API macros that are not HAS_
    text = re.sub(r"\bSMT_STEREO_HWND_API\b", "STEREO_HWND_API", text)
    text = re.sub(r"\bSMT_STEREO_API\b", "STEREO_API", text)

    # Include guards and remaining SMT_TOKEN → TOKEN (word boundary).
    # Do this after HAS_/STEREO_ so we do not double-strip.
    def strip_smt_macro(m: re.Match[str]) -> str:
        return m.group(1)

    text = re.sub(r"\bSMT_([A-Z][A-Z0-9_]*)\b", strip_smt_macro, text)

    # Wide / string HWND props already handled via TYPE_MAP for known names.
    # Local var smt_pt → pt
    text = re.sub(r"\bsmt_pt\b", "pt", text)

    # IPC frame magic comment + value
    text = text.replace(
        "0x31544D53u;  // 'SMT1' LE",
        "0x31534947u;  // 'GIS1' LE",
    )
    text = text.replace("'SMT1'", "'GIS1'")
    text = text.replace('"SMT1"', '"GIS1"')

    # Forced-include flag spelling
    text = text.replace("/FIsmt_compat.h", "/FIwin_compat.h")
    text = text.replace("smt_compat.h", "win_compat.h")

    return text


def iter_files() -> list[Path]:
    roots = [
        ROOT / "src",
        ROOT / "build",
        ROOT / "testing",
        ROOT / ".cursor" / "skills",
        ROOT / ".cursor" / "rules",
        ROOT / "docs" / "superpowers",
        ROOT / "README.md",
        ROOT / "BUILD.gn",
        ROOT / "build.bat",
    ]
    out: list[Path] = []
    for root in roots:
        if root.is_file():
            out.append(root)
            continue
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() not in TEXT_EXTS and path.name not in {
                "BUILD.gn",
                "build.bat",
            }:
                continue
            if should_skip(path):
                continue
            out.append(path)
    return out


def main() -> int:
    dry = "--dry-run" in sys.argv
    # File renames first
    for old_rel, new_rel in FILE_RENAMES:
        old_p = ROOT / old_rel
        new_p = ROOT / new_rel
        if old_p.exists() and not new_p.exists():
            print(("DRY move " if dry else "MOVE ") + old_rel + " -> " + new_rel)
            if not dry:
                new_p.parent.mkdir(parents=True, exist_ok=True)
                old_p.rename(new_p)

    changed = 0
    for path in sorted(iter_files()):
        try:
            raw = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            raw = path.read_bytes().decode("utf-8", errors="replace")
        out = rewrite_text(raw)
        if out != raw:
            changed += 1
            rel = path.relative_to(ROOT).as_posix()
            print(("DRY " if dry else "OK  ") + rel)
            if not dry:
                path.write_bytes(out.encode("utf-8"))
    print(f"files_changed={changed} dry={dry}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
