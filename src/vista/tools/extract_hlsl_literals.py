#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""One-shot: extract R\"(... )\" bodies from vista atmosphere hlsl.h into .hlsl."""

from __future__ import annotations

import re
import sys
from pathlib import Path

_RE = re.compile(
    r"inline constexpr char (k[A-Za-z0-9_]+)\[\] = R\"\((.*?)\)\";",
    re.DOTALL,
)


def _symbol_to_filename(symbol: str) -> str:
    # kVsOcean -> vs_ocean.hlsl; kCsOceanSpectrum -> cs_ocean_spectrum.hlsl
    assert symbol.startswith("k")
    body = symbol[1:]
    out: list[str] = []
    for i, ch in enumerate(body):
        if ch.isupper() and i > 0 and (body[i - 1].islower() or body[i - 1].isdigit()):
            out.append("_")
        out.append(ch.lower())
    return "".join(out) + ".hlsl"


def extract_file(hlsl_h: Path) -> list[Path]:
    text = hlsl_h.read_text(encoding="utf-8")
    written: list[Path] = []
    for m in _RE.finditer(text):
        symbol = m.group(1)
        body = m.group(2)
        # Bodies historically start with a newline after R"( ; strip one
        # leading newline so the .hlsl file starts on the first code line.
        if body.startswith("\n"):
            body = body[1:]
        if body and not body.endswith("\n"):
            body += "\n"
        out = hlsl_h.parent / _symbol_to_filename(symbol)
        out.write_text(body, encoding="utf-8", newline="\n")
        written.append(out)
        print(f"wrote {out}  ({symbol})")
    return written


def main() -> int:
    roots = [
        Path("src/vista/pass/world/atmosphere"),
    ]
    total = 0
    for root in roots:
        for hlsl_h in sorted(root.glob("*/hlsl.h")):
            total += len(extract_file(hlsl_h))
    print(f"done: {total} shaders")
    return 0


if __name__ == "__main__":
    sys.exit(main())
