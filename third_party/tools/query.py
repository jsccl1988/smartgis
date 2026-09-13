#!/usr/bin/env python3
# Copyright (c) 2018 The Mogu Authors. All rights reserved.
"""查询 manifest 中的内网包信息。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _tp_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _load_manifest(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def list_internal_packages(manifest: dict, show_all: bool = False) -> None:
    """列出有内网版本的包。"""
    packages = manifest.get("packages") or []
    internal_pkgs = [
        p for p in packages
        if p.get("internal_ref") or show_all
    ]

    if not internal_pkgs:
        print("[query] 没有找到有内网版本的包", file=sys.stderr)
        return

    print("=" * 80)
    print(f"{'包名':<20} {'上游版本':<15} {'内网位置':<15} {'内网版本':<12} 说明")
    print("=" * 80)

    for pkg in internal_pkgs:
        name = pkg.get("name", "")
        git_ref = pkg.get("git_ref", "")
        internal_ref = pkg.get("internal_ref", "N/A")
        internal_ver = pkg.get("internal_version", "N/A")
        internal_note = pkg.get("internal_note", "")[:30] if pkg.get("internal_note") else ""
        internal_name = pkg.get("internal_name", "")

        if internal_name:
            internal_ref = f"{internal_ref}/{internal_name}"

        print(f"{name:<20} {git_ref:<15} {internal_ref:<15} {internal_ver:<12} {internal_note}")


def show_package_details(manifest: dict, package_name: str) -> None:
    """显示单个包的详细信息。"""
    packages = manifest.get("packages") or []
    for pkg in packages:
        if pkg.get("name", "").lower() == package_name.lower():
            print("=" * 80)
            print(f"包名: {pkg.get('name')}")
            print(f"上游 URL: {pkg.get('git_url')}")
            print(f"上游版本: {pkg.get('git_ref')}")
            print(f"构建系统: {pkg.get('build_system')}")
            print()

            if pkg.get("internal_ref"):
                print("内网版本信息:")
                print(f"  位置: {pkg.get('internal_ref')}")
                if pkg.get("internal_name"):
                    print(f"  目录名: {pkg.get('internal_name')}")
                if pkg.get("internal_version"):
                    print(f"  版本: {pkg.get('internal_version')}")
                if pkg.get("internal_note"):
                    print(f"  说明: {pkg.get('internal_note')}")
            else:
                print("内网版本: 无")

            print()

            if pkg.get("install_requires"):
                print(f"依赖: {', '.join(pkg.get('install_requires'))}")

            if pkg.get("cmake_args"):
                print("CMake 参数:")
                for arg in pkg.get("cmake_args", []):
                    print(f"  {arg}")

            print("=" * 80)
            return

    print(f"[query] ERROR: 未找到包 {package_name!r}", file=sys.stderr)
    sys.exit(1)


def summary(manifest: dict) -> None:
    """显示统计摘要。"""
    packages = manifest.get("packages") or []
    total = len(packages)
    internal_count = sum(1 for p in packages if p.get("internal_ref"))

    print("=" * 80)
    print("包信息统计:")
    print(f"  总包数: {total}")
    print(f"  有内网版本: {internal_count}")
    print(f"  需从源码编译: {total - internal_count}")
    print()

    # 按内网位置分组
    locations = {}
    for pkg in packages:
        ref = pkg.get("internal_ref")
        if ref:
            locations[ref] = locations.get(ref, 0) + 1

    if locations:
        print("内网位置分布:")
        for loc, count in sorted(locations.items()):
            print(f"  {loc}: {count} 个包")

    print("=" * 80)


def main() -> None:
    root = _tp_root()
    default_manifest = root / "manifest.json"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=default_manifest,
        help="manifest.json 路径"
    )
    subparsers = parser.add_subparsers(dest="command", help="子命令")

    subparsers.add_parser("list", help="列出有内网版本的包")
    subparsers.add_parser("list-all", help="列出所有包（含内网信息）")
    subparsers.add_parser("summary", help="显示统计摘要")

    show_parser = subparsers.add_parser("show", help="显示包详细信息")
    show_parser.add_argument("package", help="包名")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    manifest = _load_manifest(args.manifest)

    if args.command == "list":
        list_internal_packages(manifest, show_all=False)
    elif args.command == "list-all":
        list_internal_packages(manifest, show_all=True)
    elif args.command == "show":
        show_package_details(manifest, args.package)
    elif args.command == "summary":
        summary(manifest)


if __name__ == "__main__":
    main()
