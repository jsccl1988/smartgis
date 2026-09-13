#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Rename GN dll-build defines FOO_EXPORT -> FOO_EXPORTS."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PAT = re.compile(r'"([A-Z0-9_]+_EXPORT)"')


def main() -> None:
    n = 0
    for p in (ROOT / "src").rglob("BUILD.gn"):
        text = p.read_text(encoding="utf-8")
        changed = False

        def repl(m: re.Match[str]) -> str:
            nonlocal n, changed
            name = m.group(1)
            if name.endswith("EXPORTS"):
                return m.group(0)
            n += 1
            changed = True
            return f'"{name}S"'

        new = PAT.sub(repl, text)
        if changed:
            p.write_text(new, encoding="utf-8", newline="\n")
            print(p.relative_to(ROOT).as_posix())
    print(f"renames={n}")


if __name__ == "__main__":
    main()
