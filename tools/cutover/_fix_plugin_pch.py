# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
plugin = ROOT / "src/plugin"

pat = re.compile(r'#include\s+"[^"]*stdafx\.h"')

for p in plugin.rglob("*"):
    if p.suffix.lower() not in {".cpp", ".cc", ".c"}:
        continue
    text = p.read_text(encoding="utf-8", errors="replace")
    if "stdafx.h" not in text:
        continue
    new, n = pat.subn('#include "stdafx.h"', text)
    if n:
        p.write_text(new, encoding="utf-8", newline="\n")
        print(f"{p.relative_to(ROOT)}: {n}")

for gn in plugin.rglob("BUILD.gn"):
    text = gn.read_text(encoding="utf-8", errors="replace")
    if "smt_mfc_shared_library" not in text:
        continue
    if "stdafx.cpp" not in text:
        continue
    if 'include_dirs += [ "." ]' in text or 'include_dirs += ["."]' in text:
        continue
    if "include_dirs" in text:
        continue
    # Insert after defines block if present, else after opening brace of target
    m = re.search(r"defines = \[[^\]]*\]", text, re.S)
    if m:
        insert_at = m.end()
        text = text[:insert_at] + '\n  include_dirs += [ "." ]' + text[insert_at:]
        gn.write_text(text, encoding="utf-8", newline="\n")
        print(f"include_dirs {gn.relative_to(ROOT)}")
