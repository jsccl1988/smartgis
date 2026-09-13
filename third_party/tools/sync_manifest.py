#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors. All rights reserved.
"""
将 CMake 扫描相对 manifest 多出的依赖写回 manifest.json 的 install_requires。

  python3 third_party/tools/sync_manifest.py
  python3 third_party/tools/sync_manifest.py --dry-run

需已 fetch 源码（third_party/.src/<包>/ 存在 CMakeLists.txt）。
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from deps import cmake_scan_delta_edges


def _edge_blocked_by_manifest_cmake_args(pkg: dict, dep: str) -> bool:
    """避免把 CMake 条件块里的 find_package 写进 install_requires（与 cmake_args 显式关闭一致）。"""
    args = " ".join(str(x) for x in (pkg.get("cmake_args") or []))
    compact = args.replace(" ", "")
    if dep == "gflags" and "-DWITH_GFLAGS=OFF" in compact:
        return True
    if dep == "glog" and "-DWITH_GLOG=OFF" in compact:
        return True
    if dep == "googletest" and (
        "-DWITH_GTEST=OFF" in compact or "-DBUILD_TESTING=OFF" in compact
    ):
        return True
    if dep == "benchmark" and "-DBUILD_TESTING=OFF" in compact:
        return True
    ignore = pkg.get("cmake_sync_ignore") or []
    if dep in ignore:
        return True
    return False


def _tp_root() -> Path:
    return Path(__file__).resolve().parent.parent


def main() -> None:
    tp = _tp_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=tp / "manifest.json")
    parser.add_argument("--src-root", type=Path, default=tp / ".src")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只打印变更，不写回 manifest.json",
    )
    args = parser.parse_args()

    if not args.manifest.is_file():
        print(f"[sync_manifest] ERROR: missing {args.manifest}", file=sys.stderr)
        sys.exit(1)
    if not args.src_root.is_dir():
        print(
            f"[sync_manifest] WARN: {args.src_root} missing; nothing to merge",
            file=sys.stderr,
        )
        sys.exit(0)

    text = args.manifest.read_text(encoding="utf-8")
    manifest = json.loads(text)
    delta = cmake_scan_delta_edges(manifest, args.src_root)
    if not delta:
        print("[sync_manifest] no CMake-only edges to merge (already in sync)")
        return

    by_name = {p["name"]: p for p in manifest.get("packages") or [] if p.get("name")}
    changes: list[str] = []

    for consumer in sorted(delta.keys()):
        extra = delta[consumer]
        if consumer not in by_name:
            continue
        p = by_name[consumer]
        if p.get("install_skip"):
            continue
        req = list(p.get("install_requires") or [])
        existing = set(req) | set(p.get("cmake_prefix_paths") or [])
        appended: list[str] = []
        for d in sorted(extra):
            if d == consumer or d in existing:
                continue
            if _edge_blocked_by_manifest_cmake_args(p, d):
                continue
            req.append(d)
            existing.add(d)
            appended.append(d)
        if appended:
            p["install_requires"] = req
            changes.append(f"  {consumer}: install_requires += {appended}")

    if not changes:
        print("[sync_manifest] delta non-empty but no install_requires updates (covered by cmake_prefix_paths?)")
        for c, ds in sorted(delta.items()):
            print(f"  {c}: {sorted(ds)}")
        return

    print("[sync_manifest] updates:")
    print("\n".join(changes))

    if args.dry_run:
        print("[sync_manifest] --dry-run: not writing")
        return

    args.manifest.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"[sync_manifest] wrote {args.manifest}")


if __name__ == "__main__":
    main()
