# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

reps = [
    (".is_empty()", ".IsEmpty()"),
    ("GetAppPath()", "get_app_path()"),
    ("GetParentDictory(", "get_parent_directory("),
    ('#include "datasourcemgr.h"', '#include "sdb/datasource/mgr/datasourcemgr.h"'),
    ('#include "api.h"', '#include "base/core/api.h"'),
    ('#include "xcatalog_core.h"', '#include "ui/xcatalog/xcatalog_core.h"'),
    ('#include "dlg_sel_layer.h"', '#include "ui/xcatalog/dlg_sel_layer.h"'),
    ('#include "dlg_sel_ds.h"', '#include "ui/xcatalog/dlg_sel_ds.h"'),
]

for p in (ROOT / "src/ui/xcatalog").rglob("*"):
    if p.suffix.lower() not in {".cpp", ".h", ".cc"}:
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
        print(f"{p.relative_to(ROOT)}: {local}")

pat = re.compile(r'#include\s+"[^"]*stdafx\.h"')
for p in (ROOT / "src/ui/xview").rglob("*"):
    if p.suffix.lower() not in {".cpp", ".cc", ".c"}:
        continue
    text = p.read_text(encoding="utf-8", errors="replace")
    new, n = pat.subn('#include "stdafx.h"', text)
    if n:
        p.write_text(new, encoding="utf-8", newline="\n")
        print(f"pch {p.relative_to(ROOT)}")

gn = ROOT / "src/ui/xview/BUILD.gn"
text = gn.read_text(encoding="utf-8")
if 'include_dirs += [\n    ".",\n    "//src",\n  ]' not in text:
    text2 = text.replace(
        'include_dirs += [ "//src" ]',
        'include_dirs += [\n    ".",\n    "//src",\n  ]',
    )
    if text2 != text:
        gn.write_text(text2, encoding="utf-8", newline="\n")
        print("updated xview BUILD.gn")
