#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Fix leftover PascalCase sync method calls in base/core after partial rename."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "src" / "base" / "core"

REPLACEMENTS = [
    ("->Lock(", "->lock("),
    (".Lock(", ".lock("),
    ("::Lock(", "::lock("),
    ("->Unlock(", "->unlock("),
    (".Unlock(", ".unlock("),
    ("::Unlock(", "::unlock("),
    ("->Wait(", "->wait("),
    (".Wait(", ".wait("),
    ("::Wait(", "::wait("),
    ("->Signal(", "->signal("),
    (".Signal(", ".signal("),
    ("::Signal(", "::signal("),
    ("->TryLock(", "->try_lock("),
    (".TryLock(", ".try_lock("),
]


def main() -> None:
    nfiles = 0
    for p in ROOT.glob("*.cpp"):
        text = p.read_text(encoding="utf-8", errors="replace")
        new = text
        for a, b in REPLACEMENTS:
            new = new.replace(a, b)
        if new != text:
            p.write_text(new, encoding="utf-8", newline="\n")
            nfiles += 1
            print(p.name)
    print(f"files={nfiles}")


if __name__ == "__main__":
    main()
