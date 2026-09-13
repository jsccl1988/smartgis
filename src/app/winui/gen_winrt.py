# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Run Windows SDK cppwinrt.exe over Windows App SDK winmds."""

from __future__ import print_function

import os
import subprocess
import sys


def _find_cppwinrt():
    kits = os.environ.get(
        "WindowsSdkVerBinPath",
        r"C:\Program Files (x86)\Windows Kits\10\bin")
    if os.path.isfile(os.path.join(kits, "x64", "cppwinrt.exe")):
        return os.path.join(kits, "x64", "cppwinrt.exe")

    base = r"C:\Program Files (x86)\Windows Kits\10\bin"
    if os.path.isdir(base):
        versions = sorted(os.listdir(base), reverse=True)
        for ver in versions:
            cand = os.path.join(base, ver, "x64", "cppwinrt.exe")
            if os.path.isfile(cand):
                return cand
    return None


def _collect_winmds(wasdk_root):
    # All WinAppSDK winmds except duplicate 17763 copies (18362 wins).
    # Microsoft.UI.Xaml.Controls.WebView2 references the WebView2 Core winmd
    # (projection only; this chrome does not host Edge).
    search_roots = [wasdk_root]
    parent = os.path.dirname(wasdk_root)
    if os.path.isdir(parent):
        for name in os.listdir(parent):
            if name.startswith("Microsoft.Web.WebView2."):
                search_roots.append(os.path.join(parent, name))

    best = {}
    for root in search_roots:
        for dirpath, _dirnames, filenames in os.walk(root):
            lower = dirpath.replace("\\", "/").lower()
            if "/lib/net" in lower or "/ref/" in lower or "17763" in lower:
                continue
            if "wv2winrt" in lower:
                continue
            for name in filenames:
                if not name.lower().endswith(".winmd"):
                    continue
                key = name.lower()
                path = os.path.join(dirpath, name)
                prev = best.get(key)
                if prev is None or "18362" in lower:
                    best[key] = path
    winmds = [best[k] for k in sorted(best)]
    return winmds


def main():
    if len(sys.argv) < 3:
        print("usage: gen_winrt.py <wasdk_root> <out_dir>", file=sys.stderr)
        return 2

    wasdk_root = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    cppwinrt = _find_cppwinrt()
    if not cppwinrt:
        print("ERROR: cppwinrt.exe not found under Windows Kits\\10\\bin",
              file=sys.stderr)
        print("Expected e.g. "
              r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\cppwinrt.exe",
              file=sys.stderr)
        return 2

    winmds = _collect_winmds(wasdk_root)
    if not winmds:
        print("ERROR: no .winmd under %s" % wasdk_root, file=sys.stderr)
        return 2

    cmd = [cppwinrt]
    for winmd in winmds:
        cmd.extend(["-in", winmd])
    cmd.extend(["-in", "sdk", "-out", out_dir])
    print(" ".join(cmd))
    sys.stdout.flush()
    rc = subprocess.call(cmd)
    if rc != 0:
        return rc

    stamp = os.path.join(out_dir, "winrt_projection.stamp")
    with open(stamp, "w") as f:
        f.write("ok\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
