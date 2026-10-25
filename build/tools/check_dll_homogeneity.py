# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Gate: product DLLs in out/<config> must match Debug/Release stem rules.

Chromium-style homogeneous CRT / STL trust (living §DLL / STL trust model):
  Debug  → product stems are ``{stem}_d.dll``; bare ``{stem}.dll`` must not appear
  Release → product stems are ``{stem}.dll``; ``{stem}_d.dll`` must not appear

Also refuses a mixed pair (both ``foo.dll`` and ``foo_d.dll``) in the same gen root.

When ``smartgis_build_id.txt`` exists (written by //build:gen_build_id), every
present product stem DLL must contain the matching ``SMARTGIS_BUILD_ID=…``
marker blob (cross-image mix from another build).

Usage:
  python build/tools/check_dll_homogeneity.py --out Debug
  python build/tools/check_dll_homogeneity.py --out Release
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Keep in sync with docs/superpowers/abi-rename-map.md product stems and
# src/base/core/product_build_id_check.cc kProductStems.
PRODUCT_STEMS: tuple[str, ...] = (
    "base",
    "net",
    "gis",
    "vista",
    "scenic",
    "render",
    "content",
    "tool",
    "ui_views",
    "plugin_host",
    "ui_legacy",
    "legacy_render",
    "legacy_render_gl",
    "legacy_render_d3d",
    "legacy_tool",
)


def _product_dll_path(out_dir: Path, stem: str, want_debug: bool) -> Path:
    return out_dir / (f"{stem}_d.dll" if want_debug else f"{stem}.dll")


def _collect_build_id_issues(
    out_dir: Path, want_debug: bool
) -> tuple[list[str], list[str]]:
    """Return (errors, warnings) for SMARTGIS_BUILD_ID marker homogeneity."""
    errors: list[str] = []
    warnings: list[str] = []
    stamp = out_dir / "smartgis_build_id.txt"
    if not stamp.is_file():
        warnings.append(
            f"missing {stamp.name} — skip build_id marker scan "
            "(rebuild after //build:gen_build_id lands)"
        )
        return errors, warnings

    build_id = stamp.read_text(encoding="utf-8").strip()
    if not build_id:
        errors.append(f"empty {stamp}")
        return errors, warnings

    needle = f"SMARTGIS_BUILD_ID={build_id}".encode("ascii", errors="strict")
    checked = 0
    for stem in PRODUCT_STEMS:
        dll = _product_dll_path(out_dir, stem, want_debug)
        if not dll.is_file():
            continue
        try:
            data = dll.read_bytes()
        except OSError as exc:
            errors.append(f"unreadable {dll.name}: {exc}")
            continue
        if needle not in data:
            errors.append(
                f"build_id marker missing in {dll.name} "
                f"(expected {needle.decode('ascii')}) — mixed/stale DLL"
            )
            continue
        checked += 1

    if checked == 0:
        warnings.append(
            "build_id stamp present but no product stem DLLs to scan"
        )
    return errors, warnings


def collect_issues(out_dir: Path, config: str) -> tuple[list[str], list[str]]:
    """Return (errors, warnings) for a gen root. Empty out_dir → one error."""
    errors: list[str] = []
    warnings: list[str] = []
    if not out_dir.is_dir():
        return [f"missing gen root {out_dir}"], warnings

    want_debug = config == "Debug"
    for stem in PRODUCT_STEMS:
        bare = out_dir / f"{stem}.dll"
        debug = out_dir / f"{stem}_d.dll"
        has_bare = bare.is_file()
        has_debug = debug.is_file()

        if has_bare and has_debug:
            errors.append(
                f"mixed CRT/STL risk: both {bare.name} and {debug.name} "
                f"under {out_dir}"
            )
            continue

        if want_debug:
            if has_bare and not has_debug:
                errors.append(
                    f"Debug gen root must not hold Release stem {bare.name} "
                    f"(expected {debug.name} or absent)"
                )
        else:
            if has_debug and not has_bare:
                errors.append(
                    f"Release gen root must not hold Debug stem {debug.name} "
                    f"(expected {bare.name} or absent)"
                )

    for path in sorted(out_dir.glob("*_d.dll")):
        stem = path.name[: -len("_d.dll")]
        if stem in PRODUCT_STEMS:
            continue
        sibling = out_dir / f"{stem}.dll"
        if sibling.is_file():
            warnings.append(
                f"non-catalog mixed pair: {path.name} + {sibling.name}"
            )

    id_err, id_warn = _collect_build_id_issues(out_dir, want_debug)
    errors.extend(id_err)
    warnings.extend(id_warn)
    return errors, warnings


def check(out_dir: Path, config: str) -> int:
    errors, warnings = collect_issues(out_dir, config)
    for w in warnings:
        print(f"warn: {w}")
    if errors:
        print(f"ERROR: check_dll_homogeneity --out {config} failed:")
        for e in errors:
            print(f"  - {e}")
        print(
            "  Tip: never mix Debug (_d) and Release product DLLs in one "
            "out/<config>. Rebuild that config only: build.bat "
            f"{config.lower()}"
        )
        return 1

    want_debug = config == "Debug"
    present = sum(
        1
        for s in PRODUCT_STEMS
        if _product_dll_path(out_dir, s, want_debug).is_file()
    )
    stamp = out_dir / "smartgis_build_id.txt"
    id_note = ""
    if stamp.is_file():
        id_note = f" build_id={stamp.read_text(encoding='utf-8').strip()}"
    print(
        f"check_dll_homogeneity: OK out/{config} "
        f"product_stems_present={present}/{len(PRODUCT_STEMS)}{id_note}"
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--out",
        default="Debug",
        choices=("Debug", "Release"),
        help="Gen-root config under out/",
    )
    ap.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Repo root (default: inferred from this script)",
    )
    args = ap.parse_args(argv)
    root = args.root
    if root is None:
        root = Path(__file__).resolve().parents[2]
    return check(root / "out" / args.out, args.out)


if __name__ == "__main__":
    sys.exit(main())
