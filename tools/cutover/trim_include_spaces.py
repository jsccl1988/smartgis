#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAT = re.compile(r'(#\s*include\s+")([^"]+?)(\s+)(")')
SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl"}


def main() -> None:
    n = 0
    for root in (ROOT / "src", ROOT / "testing"):
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            text = p.read_text(encoding="utf-8", errors="replace")
            new, c = PAT.subn(r"\1\2\4", text)
            if c:
                p.write_text(new, encoding="utf-8", newline="\n")
                n += c
    print(f"trimmed={n}")


if __name__ == "__main__":
    main()
