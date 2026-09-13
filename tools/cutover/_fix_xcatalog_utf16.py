# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Convert UTF-16 xcatalog sources to UTF-8 and normalize stdafx include."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "src/ui/xcatalog"
old = '#include "ui/xcatalog/stdafx.h"'
new = '#include "stdafx.h"'

for p in ROOT.glob("*.cpp"):
    raw = p.read_bytes()
    text = None
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        text = raw.decode("utf-16")
        print(f"utf16 {p.name}")
    elif b"\x00" in raw[:200]:
        text = raw.decode("utf-16-le")
        print(f"utf16le {p.name}")
    else:
        text = raw.decode("utf-8", errors="replace")
    if old in text:
        text = text.replace(old, new)
        print(f"  fixed include {p.name}")
    # Normalize newlines and write UTF-8 without BOM
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    p.write_text(text, encoding="utf-8", newline="\n")
