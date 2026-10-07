#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Embed .hlsl sources into C++ raw-string .inc files for Vista passes.

Each output .inc is a single expression:
  R"VISTA_HLSL(<file contents>)VISTA_HLSL"

Thin headers then do:
  inline constexpr char kVsOcean[] =
  #include "vista/pass/world/atmosphere/ocean/hlsl/vs_ocean.hlsl.inc"
  ;
"""

from __future__ import annotations

import argparse
import os
import sys

_DELIMITER = "VISTA_HLSL"


def _embed_one(src_path: str, out_path: str) -> None:
    with open(src_path, "r", encoding="utf-8", newline="\n") as f:
        body = f.read()
    if f"){_DELIMITER}\"" in body:
        raise SystemExit(
            f"{src_path}: contains closing delimiter ){_DELIMITER}\" — pick another"
        )
    # Preserve file text; ensure a trailing newline so the raw string matches
    # the historical R"(...)" layout (leading newline after open quote).
    if body and not body.endswith("\n"):
        body += "\n"
    text = f'R"{_DELIMITER}(\n{body}){_DELIMITER}"\n'
    out_dir = os.path.dirname(os.path.abspath(out_path))
    os.makedirs(out_dir, exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--pair",
        action="append",
        default=[],
        metavar="SRC=OUT",
        help="Embed SRC (.hlsl) into OUT (.inc). Repeatable.",
    )
    args = p.parse_args()
    if not args.pair:
        p.error("at least one --pair SRC=OUT is required")
    for pair in args.pair:
        if "=" not in pair:
            p.error(f"bad --pair (want SRC=OUT): {pair}")
        src, out = pair.split("=", 1)
        if not src or not out:
            p.error(f"bad --pair (want SRC=OUT): {pair}")
        _embed_one(src, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
