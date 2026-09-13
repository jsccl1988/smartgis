#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Sync call sites to base snake_case APIs after Task 3 partial rename."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".inl"}

# Method / free-function renames introduced in base (call-site form).
REPLACEMENTS = [
    ("GetSingletonPtr(", "get_singleton_ptr("),
    ("DestroyInstance(", "destroy_instance("),
    ("GetDefaultLog(", "get_default_log("),
    ("GetLog(", "get_log("),
    ("CreateLog(", "create_log("),
    ("LogMessage(", "log_message("),
    ("LoadDynLib(", "load_dyn_lib("),
    ("UnLoadDynLib(", "unload_dyn_lib("),
    ("GetImageTypeByFileExt(", "get_image_type_by_file_ext("),
]


def main() -> None:
    files = n = 0
    for root in (ROOT / "src", ROOT / "testing"):
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SUFFIXES:
                continue
            # Skip tinyxml Clear confusion — only call forms above
            text = p.read_text(encoding="utf-8", errors="replace")
            new = text
            local = 0
            for a, b in REPLACEMENTS:
                c = new.count(a)
                if c:
                    new = new.replace(a, b)
                    local += c
            if local:
                p.write_text(new, encoding="utf-8", newline="\n")
                files += 1
                n += local
    print(f"files={files} replacements={n}")


if __name__ == "__main__":
    main()
