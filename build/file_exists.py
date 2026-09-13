#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Print 1 if argv[1] is a file under the repo root, else 0."""

import os
import sys


def main() -> int:
    if len(sys.argv) < 2:
        print("0")
        return 0
    rel = sys.argv[1].replace("\\", "/").lstrip("/")
    if rel.startswith("//"):
        rel = rel[2:]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(root, *rel.split("/"))
    print("1" if os.path.isfile(path) else "0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
