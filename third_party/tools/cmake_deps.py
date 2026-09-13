#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""
从第三方源码树中的 CMakeLists.txt / *.cmake 抽取 find_package / find_dependency，
映射到 manifest 包名，用于与 manifest 声明合并校准安装顺序。
"""

from __future__ import annotations

import re
from pathlib import Path


def package_src_dir(
    src_root: Path, pkg: dict, package_name: str | None = None
) -> Path:
    """Resolve ``.src/<src_dir_name or name>`` the same way fetch.py places HTTP trees.

    Git clones still land in ``.src/<name>``. Prefer an existing directory so either
    layout works; if neither exists, return the ``src_dir_name`` path (HTTP expected).
    """
    name = package_name or pkg.get("name") or ""
    src_dir_name = pkg.get("src_dir_name") or name
    aliased = src_root / src_dir_name
    named = src_root / name
    if aliased.is_dir():
        return aliased
    if named.is_dir():
        return named
    # Legacy: top-level third_party/<name>/CMakeLists.txt only (not thin BUILD.gn).
    local_name = pkg.get("local_src") or name
    if local_name:
        local = src_root.parent / local_name
        try:
            if local.is_dir() and local.resolve() != src_root.resolve():
                if (local / "CMakeLists.txt").is_file():
                    return local
        except OSError:
            pass
    return aliased


# CMake 常见包名 -> manifest 包名；值为 None 表示视为系统/工具链依赖，不触发源码安装
_CMAKE_PACKAGE_MAP: dict[str, str | None] = {
    "Protobuf": "protobuf",
    "protobuf": "protobuf",
    "gRPC": "grpc",
    "grpc": "grpc",
    "GFLAGS": "gflags",
    "Gflags": "gflags",
    "gflags": "gflags",
    "GLOG": "glog",
    "glog": "glog",
    "GTest": "googletest",
    "GTEST": "googletest",
    "googletest": "googletest",
    "GoogleTest": "googletest",
    "Benchmark": "benchmark",
    "benchmark": "benchmark",
    "absl": "abseil-cpp",
    "Abseil": "abseil-cpp",
    "Fmt": "fmt",
    "fmt": "fmt",
    "spdlog": "spdlog",
    "Spdlog": "spdlog",
    "RocksDB": "rocksdb",
    "rocksdb": "rocksdb",
    "ZLIB": None,
    "OpenSSL": None,
    "MbedTLS": None,
    "Threads": None,
    "Thread": None,
    "PkgConfig": None,
    "Python": None,
    "Python3": None,
    "PythonInterp": None,
    "PythonLibs": None,
    "Git": None,
    "CUDA": None,
    "CUDAToolkit": None,
    "SWIG": None,
    "JNI": None,
    "Boost": None,
    "Doxygen": None,
    "Sphinx": None,
    "CURL": None,
    "LibXml2": None,
    "SQLite3": "sqlite3",
    "PostgreSQL": None,
    "MySQL": None,
    "leveldb": None,
    "LevelDB": None,
    "Snappy": None,
    "snappy": None,
    "LZ4": None,
    "BZip2": None,
    "nlohmann_json": None,
    "JsonCpp": None,
}

_FIND_PACKAGE_RE = re.compile(
    r"^\s*find_package\s*\(\s*([A-Za-z0-9_]+)",
    re.MULTILINE | re.IGNORECASE,
)
_FIND_DEPENDENCY_RE = re.compile(
    r"^\s*find_dependency\s*\(\s*([A-Za-z0-9_]+)",
    re.MULTILINE | re.IGNORECASE,
)


def _resolve_manifest_name(raw: str, manifest_names: set[str]) -> str | None:
    raw = raw.strip()
    if not raw:
        return None
    mapped = _CMAKE_PACKAGE_MAP.get(raw)
    if mapped is not None:
        return mapped if mapped in manifest_names else None
    if raw in _CMAKE_PACKAGE_MAP and _CMAKE_PACKAGE_MAP[raw] is None:
        return None
    # 直接命中 manifest（大小写/连字符）
    if raw in manifest_names:
        return raw
    rl = raw.lower().replace("_", "-")
    for mn in manifest_names:
        if mn.lower() == rl or mn.lower().replace("-", "_") == raw.lower():
            return mn
    return None


def _collect_cmake_files(package_src: Path, cmake_source_subdir: str | None) -> list[Path]:
    roots: list[Path] = [package_src]
    if cmake_source_subdir:
        roots.append(package_src / cmake_source_subdir)
    files: list[Path] = []
    seen: set[Path] = set()
    for root in roots:
        if not root.is_dir():
            continue
        for pattern in ("CMakeLists.txt",):
            p = root / pattern
            if p.is_file():
                if p not in seen:
                    seen.add(p)
                    files.append(p)
        cmake_dir = root / "cmake"
        if cmake_dir.is_dir():
            for p in sorted(cmake_dir.glob("*.cmake")):
                if p.is_file() and p not in seen:
                    seen.add(p)
                    files.append(p)
        # 一层子目录下的 CMakeLists（如 example/ 常含可选依赖，略保守但可校准）
        try:
            for child in sorted(root.iterdir()):
                if not child.is_dir():
                    continue
                if child.name in (".git", "build", "out", "cmake-build-debug", "third_party"):
                    continue
                sub = child / "CMakeLists.txt"
                if sub.is_file() and sub not in seen:
                    seen.add(sub)
                    files.append(sub)
        except OSError:
            pass
    return files


def scan_cmake_manifest_deps(
    *,
    package_name: str,
    manifest: dict,
    src_root: Path,
) -> set[str]:
    """返回该包 CMake 中引用、且出现在 manifest 中的第三方包名（不含自身）。"""
    names = {p["name"] for p in manifest.get("packages") or [] if p.get("name")}
    if package_name not in names:
        return set()

    pkg_def = next(
        (p for p in (manifest.get("packages") or []) if p.get("name") == package_name),
        None,
    )
    if not pkg_def or pkg_def.get("install_skip"):
        return set()

    package_src = package_src_dir(src_root, pkg_def, package_name)
    if not package_src.is_dir():
        return set()

    sub = pkg_def.get("cmake_source_subdir")
    found: set[str] = set()
    for path in _collect_cmake_files(package_src, sub):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for rx in (_FIND_PACKAGE_RE, _FIND_DEPENDENCY_RE):
            for m in rx.finditer(text):
                raw = m.group(1)
                mn = _resolve_manifest_name(raw, names)
                if mn and mn != package_name:
                    found.add(mn)
    return found


# 常见于 find_package 但多用于测试/示例；不自动纳入「必装」图，避免误装（manifest 仍可 install_requires）
_CMAKE_SCAN_SKIP_AUTO_EDGE: frozenset[str] = frozenset({"googletest", "benchmark"})


def merge_cmake_predecessors(
    preds: dict[str, set[str]],
    manifest: dict,
    src_root: Path,
    *,
    log: bool = False,
) -> int:
    """就地合并 CMake 扫描边。返回新增边条数。log=True 时逐条打印。"""
    added = 0
    for consumer in list(preds.keys()):
        extra = scan_cmake_manifest_deps(
            package_name=consumer, manifest=manifest, src_root=src_root
        )
        for d in extra:
            if d == consumer or d in _CMAKE_SCAN_SKIP_AUTO_EDGE:
                continue
            if d not in preds:
                continue
            if not _install_skip_local(manifest, d):
                if d not in preds[consumer]:
                    preds[consumer].add(d)
                    added += 1
                    if log:
                        print(
                            f"[deps] cmake-calibrate: {consumer} -> {d} (CMake scan)",
                            flush=True,
                        )
    if added and not log:
        print(
            f"[deps] cmake-calibrate: +{added} edge(s) from CMake scan (set INCUBATOR_TP_DEPS_LOG=1 for details)",
            flush=True,
        )
    return added


def manifest_names_set(manifest: dict) -> set[str]:
    return {p["name"] for p in manifest.get("packages") or [] if p.get("name")}


def _install_skip_local(manifest: dict, name: str) -> bool:
    for p in manifest.get("packages") or []:
        if p.get("name") == name:
            return bool(p.get("install_skip"))
    return False
