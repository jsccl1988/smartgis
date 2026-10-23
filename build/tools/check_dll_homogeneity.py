# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Gate: product DLLs in out/<config> must match Debug/Release stem rules.

Chromium-style homogeneous CRT / STL trust (living §DLL / STL trust model):
  Debug  → product stems are ``{stem}_d.dll``; bare ``{stem}.dll`` must not appear
  Release → product stems are ``{stem}.dll``; ``{stem}_d.dll`` must not appear

Also refuses a mixed pair (both ``foo.dll`` and ``foo_d.dll``) in the same gen root.

Usage:
  python build/tools/check_dll_homogeneity.py --out Debug
  python build/tools/check_dll_homogeneity.py --out Release
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Keep in sync with docs/superpowers/abi-rename-map.md product stems.
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
        if (
            out_dir / f"{s}_d.dll" if want_debug else out_dir / f"{s}.dll"
        ).is_file()
    )
    print(
        f"check_dll_homogeneity: OK out/{config} "
        f"product_stems_present={present}/{len(PRODUCT_STEMS)}"
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
