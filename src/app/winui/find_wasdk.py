# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Print the extracted Windows App SDK root, or MISSING."""

from __future__ import print_function

import os
import sys


def _repo_root():
    here = os.path.abspath(os.path.dirname(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", ".."))


def _has_bootstrap_header(root):
    candidates = [
        os.path.join(root, "include", "MddBootstrap.h"),
        os.path.join(root, "build", "native", "include", "MddBootstrap.h"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return True
    return False


def main():
    dest_root = os.path.join(_repo_root(), "third_party", "windows_app_sdk")
    if not os.path.isdir(dest_root):
        print("MISSING")
        return 0

    version_file = os.path.join(dest_root, "VERSION")
    if os.path.isfile(version_file):
        with open(version_file, "r") as f:
            for line in f:
                line = line.strip()
                if line.startswith("root="):
                    root = line[5:].replace("/", os.sep)
                    if _has_bootstrap_header(root):
                        print(root.replace("\\", "/"))
                        return 0

    names = sorted(os.listdir(dest_root), reverse=True)
    for name in names:
        if not name.startswith("Microsoft.WindowsAppSDK."):
            continue
        root = os.path.join(dest_root, name)
        if os.path.isdir(root) and _has_bootstrap_header(root):
            print(root.replace("\\", "/"))
            return 0

    print("MISSING")
    return 0


if __name__ == "__main__":
    sys.exit(main())
