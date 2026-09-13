# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Locate BCGControlBar Pro (BCGCBProInc.h). Prints the directory that
# contains the header, or nothing if not found. Does not download BCG.

import os
import sys

CANDIDATES = [
    "third_party/bcg",
    "third_party/bcg/include",
    "third_party/BCGCBPro",
    "third_party/BCGControlBarPro/BCGCBPro",
    "third_party/BCGSoft/BCGControlBarPro/BCGCBPro",
]

ABS_ROOTS = [
    r"C:\BCGSoft",
    r"C:\Program Files\BCGSoft",
    r"C:\Program Files (x86)\BCGSoft",
    r"C:\Dev\src\gis\mgis\third_party\bcg",
    r"C:\Dev\src\gis\mgis\third_party\BCGCBPro",
    r"C:\Dev\SDK\BCG",
    r"D:\BCGSoft",
    r"D:\SDK\BCG",
]


def has_header(d):
    if not d or not os.path.isdir(d):
        return False
    if os.path.isfile(os.path.join(d, "BCGCBProInc.h")):
        return True
    return os.path.isfile(os.path.join(d, "include", "BCGCBProInc.h"))


def walk_for_header(root, max_depth=4):
    root = os.path.abspath(root)
    if not os.path.isdir(root):
        return None
    for dirpath, dirnames, filenames in os.walk(root):
        rel = os.path.relpath(dirpath, root)
        depth = 0 if rel == "." else rel.count(os.sep) + 1
        if depth > max_depth:
            dirnames[:] = []
            continue
        if "BCGCBProInc.h" in filenames:
            return dirpath
    return None


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    for rel in CANDIDATES:
        p = os.path.join(repo, rel)
        if has_header(p):
            print(os.path.abspath(p))
            return 0
        found = walk_for_header(p, 2) if os.path.isdir(p) else None
        if found:
            print(found)
            return 0
    env = os.environ.get("BCG_ROOT") or os.environ.get("BCGCBPRO")
    if env and has_header(env):
        print(os.path.abspath(env))
        return 0
    if env:
        found = walk_for_header(env, 3)
        if found:
            print(found)
            return 0
    for root in ABS_ROOTS:
        if has_header(root):
            print(os.path.abspath(root))
            return 0
        found = walk_for_header(root, 4)
        if found:
            print(found)
            return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
