# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

reps = [
    ("SetName(", "set_name("),
    ("AppendFuncItems(", "append_func_items("),
    ("AppendMsg(", "append_msg("),
    ("EnvelopeToRect(", "envelope_to_rect("),
]

files = n = 0
for p in (ROOT / "src/tool/group").rglob("*"):
    if p.suffix.lower() not in {".cpp", ".cc", ".c", ".h", ".hpp"}:
        continue
    text = p.read_text(encoding="utf-8", errors="replace")
    new = text
    local = 0
    for a, b in reps:
        c = new.count(a)
        if c:
            new = new.replace(a, b)
            local += c
    if local:
        p.write_text(new, encoding="utf-8", newline="\n")
        files += 1
        n += local
        print(f"{p.relative_to(ROOT)}: {local}")
print(f"tool_group files={files} replacements={n}")

old = '#include "ui/xcatalog/stdafx.h"'
new_inc = '#include "stdafx.h"'
for p in (ROOT / "src/ui/xcatalog").rglob("*"):
    if p.suffix.lower() not in {".cpp", ".cc", ".c"}:
        continue
    text = p.read_text(encoding="utf-8", errors="replace")
    if old in text:
        p.write_text(text.replace(old, new_inc), encoding="utf-8", newline="\n")
        print(f"pch {p.relative_to(ROOT)}")
