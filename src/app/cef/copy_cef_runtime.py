# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Copy CEF Binary Dist runtime files next to SmartGisCef.exe."""

from __future__ import annotations

import os
import shutil
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print(
            "usage: copy_cef_runtime.py <cef_binary_root> <out_dir> [Debug|Release]",
            file=sys.stderr,
        )
        return 2
    src_root = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    config = "Release"
    if len(sys.argv) >= 4 and sys.argv[3] in ("Debug", "Release"):
        config = sys.argv[3]
    os.makedirs(out_dir, exist_ok=True)

    bin_dir = os.path.join(src_root, config)
    if not os.path.isdir(bin_dir):
        bin_dir = os.path.join(src_root, "Release")
    resources = os.path.join(src_root, "Resources")
    if not os.path.isdir(bin_dir):
        print(f"CEF {config}/ missing under {src_root}", file=sys.stderr)
        return 1

    for name in os.listdir(bin_dir):
        path = os.path.join(bin_dir, name)
        if os.path.isfile(path) and not name.endswith(".lib"):
            shutil.copy2(path, os.path.join(out_dir, name))

    if os.path.isdir(resources):
        for name in os.listdir(resources):
            path = os.path.join(resources, name)
            dest = os.path.join(out_dir, name)
            if os.path.isdir(path):
                if os.path.isdir(dest):
                    shutil.rmtree(dest)
                shutil.copytree(path, dest)
            elif os.path.isfile(path):
                shutil.copy2(path, dest)

    locales_src = os.path.join(bin_dir, "locales")
    if not os.path.isdir(locales_src):
        locales_src = os.path.join(resources, "locales")
    if os.path.isdir(locales_src):
        locales_dst = os.path.join(out_dir, "locales")
        if os.path.isdir(locales_dst):
            shutil.rmtree(locales_dst)
        shutil.copytree(locales_src, locales_dst)

    stamp = os.path.join(out_dir, ".cef_runtime_stamp")
    with open(stamp, "w", encoding="utf-8") as f:
        f.write(f"ok {config}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
