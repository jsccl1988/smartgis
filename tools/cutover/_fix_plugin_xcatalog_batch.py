# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Batch-fix remaining cutover call sites for green build."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

REPS = [
    ("SetName(", "set_name("),
    ("AppendFuncItems(", "append_func_items("),
    ("AppendMsg(", "append_msg("),
    ("GetSingletonPtr(", "get_singleton_ptr("),
    ("SplitFileName(", "split_file_name("),
    ("STR_Tokenize(", "str_tokenize("),
    ("SmtPostListenerMsg(", "smt_post_listener_msg("),
    ("SmtPostAMMsg(", "SmtPostAMMsg("),  # keep for now
    (".Empty()", ".is_empty()"),
    (".AddPointCollection(", ".add_point_collection("),
    (".AddTriangleCollection(", ".add_triangle_collection("),
    ('#include "map.h"', '#include "sdb/map/map.h"'),
    ('#include "mapmgr.h"', '#include "ui/xcatalog/mapmgr.h"'),
    ('#include "sysmanager.h"', '#include "sys/sysmanager.h"'),
    ('#include "feature_api.h"', '#include "sdb/feature/feature_api.h"'),
    ('#include "logmanager.h"', '#include "base/core/logmanager.h"'),
    ("using namespace Smt_SDEDevMgr;", "// using namespace Smt_SDEDevMgr;"),
    ("using namespace Smt_GIS;", "// using namespace Smt_GIS;"),
    ("using namespace Smt_Sys;", "// using namespace Smt_Sys;"),
    ("SMT_EXPORT_DLL", "__declspec(dllexport)"),
]

scopes = [
    ROOT / "src/plugin",
    ROOT / "src/ui/xcatalog",
]

files = n = 0
for scope in scopes:
    for p in scope.rglob("*"):
        if p.suffix.lower() not in {".cpp", ".cc", ".c", ".h", ".hpp"}:
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        new = text
        local = 0
        for a, b in REPS:
            if a == b:
                continue
            c = new.count(a)
            if c:
                new = new.replace(a, b)
                local += c
        if local:
            p.write_text(new, encoding="utf-8", newline="\n")
            files += 1
            n += local
            print(f"{p.relative_to(ROOT)}: {local}")

print(f"files={files} replacements={n}")
