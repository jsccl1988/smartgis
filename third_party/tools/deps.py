#!/usr/bin/env python3
# Copyright (c) 2018 The Mogu Authors. All rights reserved.
"""从 manifest 推导第三方包安装顺序（install_requires + cmake_prefix_paths + 补充边）。"""

from __future__ import annotations

import os
import sys
from pathlib import Path

from cmake_deps import merge_cmake_predecessors

# (依赖项, 消费者) — manifest 未声明时的兜底
_EXTRA_INSTALL_EDGES: list[tuple[str, str]] = []


def manifest_names(manifest: dict) -> set[str]:
    return {p["name"] for p in manifest.get("packages") or [] if p.get("name")}


def _install_skip(manifest: dict, name: str) -> bool:
    for p in manifest.get("packages") or []:
        if p.get("name") == name:
            return bool(p.get("install_skip"))
    return False


def _opt_in(manifest: dict, name: str) -> bool:
    for p in manifest.get("packages") or []:
        if p.get("name") == name:
            return bool(p.get("opt_in"))
    return False


def predecessor_map_manifest_only(manifest: dict) -> dict[str, set[str]]:
    """仅 manifest：install_requires + cmake_prefix_paths + _EXTRA_INSTALL_EDGES（无 CMake 扫描）。"""
    names = manifest_names(manifest)
    preds: dict[str, set[str]] = {n: set() for n in names}
    for p in manifest.get("packages") or []:
        n = p.get("name")
        if not n or n not in preds or p.get("install_skip"):
            continue
        for d in (p.get("install_requires") or []) + (p.get("cmake_prefix_paths") or []):
            if d in names and not _install_skip(manifest, d):
                preds[n].add(d)
    for a, b in _EXTRA_INSTALL_EDGES:
        if a in names and b in names and not _install_skip(manifest, a):
            preds[b].add(a)
    return preds


def cmake_scan_delta_edges(manifest: dict, src_root: Path) -> dict[str, set[str]]:
    """CMake 扫描相对「仅 manifest」多出的直接依赖边；用于回写 install_requires。"""
    base = predecessor_map_manifest_only(manifest)
    full = {k: set(v) for k, v in base.items()}
    merge_cmake_predecessors(full, manifest, src_root, log=False)
    return {n: full[n] - base[n] for n in full if full[n] - base[n]}


def predecessor_map(
    manifest: dict,
    *,
    src_root: Path | None = None,
    cmake_calibrate: bool = True,
) -> dict[str, set[str]]:
    """preds[consumer] = 必须先安装的包名集合（manifest + 可选 CMake 扫描校准）。"""
    preds = predecessor_map_manifest_only(manifest)
    if cmake_calibrate and src_root is not None and src_root.is_dir():
        merge_cmake_predecessors(
            preds,
            manifest,
            src_root,
            log=os.environ.get("INCUBATOR_TP_DEPS_LOG", "").strip() not in ("", "0", "false"),
        )
    return preds


def topo_install_order(
    target: str,
    manifest: dict,
    *,
    src_root: Path | None = None,
    cmake_calibrate: bool = True,
) -> list[str]:
    """返回安装 target 及其全部传递依赖的线性顺序（依赖在前）。"""
    preds = predecessor_map(
        manifest, src_root=src_root, cmake_calibrate=cmake_calibrate
    )
    if target not in preds:
        return [target]
    if _install_skip(manifest, target):
        raise ValueError(f"package {target!r} has install_skip")

    need: set[str] = {target}
    stack = [target]
    while stack:
        n = stack.pop()
        for d in preds.get(n, ()):
            if _install_skip(manifest, d):
                continue
            if d not in need:
                need.add(d)
                stack.append(d)

    order: list[str] = []
    remaining = set(need)
    completed: set[str] = set()
    while remaining:
        ready = sorted(n for n in remaining if preds.get(n, set()) <= completed)
        if not ready:
            raise ValueError(f"install dependency cycle among {remaining}")
        order.extend(ready)
        completed.update(ready)
        remaining -= set(ready)
    return order


def _sort_ready(ready: list[str], preferred: list[str] | None) -> None:
    if not preferred:
        ready.sort()
        return
    pref_idx = {n: i for i, n in enumerate(preferred)}
    ready.sort(key=lambda n: (pref_idx.get(n, 10_000), n))


def topo_batches_all_packages(
    manifest: dict,
    *,
    preferred: list[str] | None = None,
    src_root: Path | None = None,
    cmake_calibrate: bool = True,
    preds: dict[str, set[str]] | None = None,
) -> list[list[str]]:
    """install_all：对全部可安装包分层（同层可并行）。"""
    pkgs = [
        p
        for p in manifest.get("packages") or []
        if p.get("name") and not p.get("install_skip") and not p.get("opt_in")
    ]
    names = [p["name"] for p in pkgs]
    if preds is None:
        preds = predecessor_map(
            manifest, src_root=src_root, cmake_calibrate=cmake_calibrate
        )

    batches: list[list[str]] = []
    completed: set[str] = set()
    remaining = set(names)
    while remaining:
        ready = [n for n in remaining if preds.get(n, set()) <= completed]
        if not ready:
            print(
                f"[install_all] ERROR: dependency cycle or missing order for {remaining}",
                file=sys.stderr,
            )
            sys.exit(1)
        _sort_ready(ready, preferred)
        batches.append(ready)
        completed.update(ready)
        remaining -= set(ready)
    return batches


def closure_requested_packages(
    requested_lower: set[str],
    manifest: dict,
    *,
    src_root: Path | None = None,
    cmake_calibrate: bool = True,
    preds: dict[str, set[str]] | None = None,
) -> set[str]:
    """CLI --package 小写名 -> 规范名 + 全部传递依赖（跳过 install_skip 依赖链）。"""
    if preds is None:
        preds = predecessor_map(
            manifest, src_root=src_root, cmake_calibrate=cmake_calibrate
        )
    by_l = {p["name"].lower(): p["name"] for p in manifest.get("packages") or [] if p.get("name")}
    need: set[str] = set()
    for r in requested_lower:
        if r not in by_l:
            print(f"[install_all] ERROR: unknown package {r!r}", file=sys.stderr)
            sys.exit(1)
        n = by_l[r]
        if _install_skip(manifest, n):
            print(f"[install_all] ERROR: package {n!r} has install_skip", file=sys.stderr)
            sys.exit(1)
        need.add(n)

    stack = list(need)
    while stack:
        n = stack.pop()
        for d in preds.get(n, ()):
            if _install_skip(manifest, d):
                continue
            if d not in need:
                need.add(d)
                stack.append(d)
    return need


def topo_batches_subset(
    packages: set[str],
    manifest: dict,
    *,
    preferred: list[str] | None,
    src_root: Path | None = None,
    cmake_calibrate: bool = True,
    preds: dict[str, set[str]] | None = None,
) -> list[list[str]]:
    """仅安装 packages 子集时的分层（含传递依赖闭包后的集合）。"""
    packages = {n for n in packages if not _install_skip(manifest, n)}
    if preds is None:
        preds = predecessor_map(
            manifest, src_root=src_root, cmake_calibrate=cmake_calibrate
        )
    batches: list[list[str]] = []
    completed: set[str] = set()
    remaining = set(packages)
    while remaining:
        ready = [n for n in remaining if preds.get(n, set()) <= completed]
        if not ready:
            print(
                f"[install_all] ERROR: dependency cycle in subset {remaining}",
                file=sys.stderr,
            )
            sys.exit(1)
        _sort_ready(ready, preferred)
        batches.append(ready)
        completed.update(ready)
        remaining -= set(ready)
    return batches
