# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]

REPS = [
    ("GetSysStyleConfig(", "get_sys_style_config("),
    ("GetSysPra(", "get_sys_pra("),
    ("SetSysPra(", "set_sys_pra("),
    ("AppendListenerMenu(", "append_listener_menu("),
    ("CreateListenerMenu(", "create_listener_menu("),
    ("LoadAllPlugin(", "load_all_plugin("),
    ("GetName(", "get_name("),
    ("->Notify(", "->notify("),
    ("->Register(", "->register_listener("),  # may need verify
    ("->UnRegister(", "->unregister_listener("),
    ("UnRegister(", "unregister_listener("),
    ("Register(", "register_listener("),
]

# Verify listener API names first via a quick print after
for scope in ["src/ui/xview", "src/plugin", "src/ui/xcatalog", "src/tool"]:
    for p in (ROOT / scope).rglob("*"):
        if p.suffix.lower() not in {".cpp", ".cc", ".c", ".h", ".hpp"}:
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        new = text
        local = 0
        for a, b in REPS:
            # Avoid rewriting register_listener that already exists
            if a.startswith("Register(") and "register_listener(" in new and a not in new:
                continue
            c = new.count(a)
            if c:
                new = new.replace(a, b)
                local += c
        # Notify method decls/defs
        new2, c2 = re.subn(r"(?<![A-Za-z0-9_])Notify\s*\(", "notify(", new)
        local += c2
        new = new2
        if local and new != text:
            p.write_text(new, encoding="utf-8", newline="\n")
            print(f"{p.relative_to(ROOT)}: {local}")

print("done")
