# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Print the extracted WIL include parent, or MISSING."""

from __future__ import print_function

import os
import sys


def _repo_root():
    here = os.path.abspath(os.path.dirname(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", ".."))


def _has_wil(root):
    return os.path.isfile(os.path.join(root, "include", "wil", "resource.h"))


def main():
    dest_root = os.path.join(_repo_root(), "third_party", "windows_app_sdk")
    if not os.path.isdir(dest_root):
        print("MISSING")
        return 0

    names = sorted(os.listdir(dest_root), reverse=True)
    for name in names:
        if not name.startswith("Microsoft.Windows.ImplementationLibrary."):
            continue
        root = os.path.join(dest_root, name)
        if os.path.isdir(root) and _has_wil(root):
            print(root.replace("\\", "/"))
            return 0

    print("MISSING")
    return 0


if __name__ == "__main__":
    sys.exit(main())
