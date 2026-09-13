#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors. All rights reserved.
"""
Batch install manifest packages.

Legacy（与 out 并列）:
  python3 third_party/tools/batch.py --out out

独立合并前缀（<仓库>/third_party/.install，与主工程 GN/CMake 分离）:
  python3 third_party/tools/batch.py \\
    --install-prefix third_party/.install --jobs 8

  python3 third_party/tools/fetch.py --all
  python3 third_party/tools/batch.py --out out
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path

from deps import (
    closure_requested_packages,
    predecessor_map,
    topo_batches_all_packages,
    topo_batches_subset,
)
from install import default_cmake_generator


def _repo_root() -> Path:
    return Path(__file__).resolve().parent.parent.parent


def _tp_root() -> Path:
    return Path(__file__).resolve().parent.parent


_PREFERRED_ORDER = [
    "gflags",
    "glog",
    "protobuf",
    "fmt",
    "double-conversion",
    "abseil-cpp",
    "googletest",
    "benchmark",
    "spdlog",
    "sqlite3",
    "cpp-httplib",
    "geos",
    "PROJ",
    "SQLiteCpp",
    "libspatialindex",
    "gdal",
    "libspatialite",
    "SpatiaLiteCpp",
    "rocksdb",
    "grpc",
    "brpc",
    "rapidjson",
    "folly",
    "eigen",
    "faiss",
    "scann",
    "yaml-cpp",
    "sqlpp11",
    "tabulate",
    "antlr4",
]


def _run_install(
    *,
    install_py: Path,
    manifest: Path,
    src_root: Path,
    pkg_name: str,
    build_dir: Path,
    install_prefix: Path,
    stamp: Path,
    generator: str,
    build_type: str,
    clean_install: bool,
    unified_prefix: bool,
    cwd: Path,
    no_deps: bool = True,
    internal_root: str | None = None,
    prebuilt_roots: list[Path] | None = None,
) -> int:
    cmd = [
        sys.executable,
        str(install_py),
        "--manifest",
        str(manifest.resolve()),
        "--package",
        pkg_name,
        "--src-root",
        str(src_root.resolve()),
        "--build-root",
        str(build_dir.resolve()),
        "--install-prefix",
        str(install_prefix.resolve()),
        "--stamp",
        str(stamp.resolve()),
        "--generator",
        generator,
        "--build-type",
        build_type,
    ]
    if clean_install:
        cmd.append("--clean-install")
    if unified_prefix:
        cmd.append("--unified-prefix")
    if no_deps:
        cmd.append("--no-deps")
    print("+", " ".join(cmd), flush=True)
    env = os.environ.copy()
    if internal_root:
        print(f"[batch] setting INCUBATOR_TP_INTERNAL_ROOT={internal_root}", flush=True)
        env["INCUBATOR_TP_INTERNAL_ROOT"] = internal_root
    if prebuilt_roots:
        prebuilt_paths = ":".join(str(p) for p in prebuilt_roots)
        print(f"[batch] setting INCUBATOR_TP_PREBUILT_ROOTS={prebuilt_paths}", flush=True)
        env["INCUBATOR_TP_PREBUILT_ROOTS"] = prebuilt_paths
    r = subprocess.run(cmd, cwd=cwd, env=env, check=False)
    return r.returncode


def main() -> None:
    root = _repo_root()
    tp = _tp_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=tp / "manifest.json")
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="传统布局: <out>/third_party/<pkg>/（与 install_all 历史行为一致；未指定 --install-prefix 时默认 <repo>/out）",
    )
    parser.add_argument(
        "--install-prefix",
        type=Path,
        default=None,
        help="合并安装前缀（如 third_party/.install）；指定后与主工程 out 分离",
    )
    parser.add_argument(
        "--build-root",
        type=Path,
        default=None,
        help="各包构建目录父路径；默认同级 .build（合并前缀模式下为 <parent>/.build/<pkg>）",
    )
    parser.add_argument(
        "--generator",
        default=default_cmake_generator(),
        help="CMake -G (default: Ninja if on PATH, else Unix Makefiles)",
    )
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--package", action="append", default=[])
    parser.add_argument("--clean-install", action="store_true")
    parser.add_argument(
        "--jobs",
        type=int,
        default=1,
        help="并行包数量上限（按依赖分层；层内并行。默认 1 即顺序执行）",
    )
    parser.add_argument(
        "--no-cmake-calibrate",
        action="store_true",
        help="禁用 CMake 源码扫描校准，仅使用 manifest",
    )
    args = parser.parse_args()

    # 检测内部环境第三方库路径和预编译产物目录
    internal_root = None
    internal_paths = []
    # 尝试从当前仓库根目录往上查找 baidu 目录
    for ancestor in [root, root.parent, root.parent.parent, root.parent.parent.parent]:
        internal_tp_path = ancestor / "baidu" / "third-party"
        if internal_tp_path.is_dir():
            internal_paths.append(str(internal_tp_path))
            break
    for ancestor in [root, root.parent, root.parent.parent, root.parent.parent.parent]:
        internal_base_path = ancestor / "baidu" / "base"
        if internal_base_path.is_dir():
            internal_paths.append(str(internal_base_path))
            break
    if internal_paths:
        internal_root = ":".join(internal_paths)

    # 获取预编译产物目录
    prebuilt_roots = []
    prebuilt_env = os.environ.get("INCUBATOR_TP_PREBUILT_ROOTS", "")
    if prebuilt_env:
        for root in prebuilt_env.split(":"):
            if Path(root).is_dir():
                prebuilt_roots.append(Path(root))
    else:
        # 默认相对于主工程根目录的预编译产物目录
        master_root = root.parent.parent.parent
        third_64 = master_root / "third-64"
        thirdsrc = master_root / "thirdsrc"
        if third_64.is_dir():
            prebuilt_roots.append(third_64)
        if thirdsrc.is_dir():
            prebuilt_roots.append(thirdsrc)

    install_py = tp / "tools" / "install.py"
    src_root = tp / ".src"
    want = {p.lower() for p in args.package} if args.package else None
    cal = not args.no_cmake_calibrate
    src_for_cal = src_root if src_root.is_dir() else None

    with args.manifest.open(encoding="utf-8") as f:
        manifest = json.load(f)

    shared_preds = predecessor_map(
        manifest, src_root=src_for_cal, cmake_calibrate=cal
    )
    if want:
        need = closure_requested_packages(
            want,
            manifest,
            preds=shared_preds,
        )
        batches = topo_batches_subset(
            need,
            manifest,
            preferred=_PREFERRED_ORDER,
            preds=shared_preds,
        )
    else:
        batches = topo_batches_all_packages(
            manifest,
            preferred=_PREFERRED_ORDER,
            preds=shared_preds,
        )

    unified = args.install_prefix is not None
    if unified:
        install_prefix = args.install_prefix.resolve()
        build_parent = (
            args.build_root.resolve()
            if args.build_root is not None
            else install_prefix.parent / ".build"
        )
        if args.clean_install:
            if install_prefix.exists():
                shutil.rmtree(install_prefix)
            if build_parent.exists():
                shutil.rmtree(build_parent)
            install_prefix.mkdir(parents=True, exist_ok=True)
            build_parent.mkdir(parents=True, exist_ok=True)
        cwd = root
        out_tp = install_prefix  # 仅用于 done 日志
    else:
        out = (args.out or root / "out").resolve()
        out.mkdir(parents=True, exist_ok=True)
        out_tp = out / "third_party"
        out_tp.mkdir(parents=True, exist_ok=True)
        install_prefix = None  # type: ignore[assignment]
        build_parent = None
        cwd = out

    jobs = max(1, int(args.jobs))
    if unified and jobs > 1:
        print(
            "[install_all] unified install-prefix: 多包并行安装同一前缀不安全，"
            "已改为顺序执行（各包内部仍由 Ninja/CMake 并行编译）",
            flush=True,
        )
        jobs = 1

    for batch in batches:
        if jobs <= 1:
            for name in batch:
                if unified:
                    assert install_prefix is not None and build_parent is not None
                    bdir = build_parent / name
                    stamp = bdir / ".gn_publish_stamp"
                    rc = _run_install(
                        install_py=install_py,
                        manifest=args.manifest,
                        src_root=src_root,
                        pkg_name=name,
                        build_dir=bdir,
                        install_prefix=install_prefix,
                        stamp=stamp,
                        generator=args.generator,
                        build_type=args.build_type,
                        clean_install=args.clean_install,
                        unified_prefix=True,
                        cwd=cwd,
                        no_deps=True,
                        internal_root=internal_root,
                        prebuilt_roots=prebuilt_roots,
                    )
                else:
                    stamp = out_tp / name / ".gn_publish_stamp"
                    bdir = out_tp / "build" / name
                    prefix = out_tp / name
                    rc = _run_install(
                        install_py=install_py,
                        manifest=args.manifest,
                        src_root=src_root,
                        pkg_name=name,
                        build_dir=bdir,
                        install_prefix=prefix,
                        stamp=stamp,
                        generator=args.generator,
                        build_type=args.build_type,
                        clean_install=args.clean_install,
                        unified_prefix=False,
                        cwd=cwd,
                        no_deps=True,
                        internal_root=internal_root,
                        prebuilt_roots=prebuilt_roots,
                    )
                if rc != 0:
                    sys.exit(rc)
            continue

        # 并行：层内最多 jobs 个
        pending: set = set()
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            for name in batch:
                if unified:
                    assert install_prefix is not None and build_parent is not None
                    bdir = build_parent / name
                    stamp = bdir / ".gn_publish_stamp"
                    fut = ex.submit(
                        _run_install,
                        install_py=install_py,
                        manifest=args.manifest,
                        src_root=src_root,
                        pkg_name=name,
                        build_dir=bdir,
                        install_prefix=install_prefix,
                        stamp=stamp,
                        generator=args.generator,
                        build_type=args.build_type,
                        clean_install=args.clean_install,
                        unified_prefix=True,
                        cwd=cwd,
                        no_deps=True,
                    )
                else:
                    stamp = out_tp / name / ".gn_publish_stamp"
                    bdir = out_tp / "build" / name
                    prefix = out_tp / name
                    fut = ex.submit(
                        _run_install,
                        install_py=install_py,
                        manifest=args.manifest,
                        src_root=src_root,
                        pkg_name=name,
                        build_dir=bdir,
                        install_prefix=prefix,
                        stamp=stamp,
                        generator=args.generator,
                        build_type=args.build_type,
                        clean_install=args.clean_install,
                        unified_prefix=False,
                        cwd=cwd,
                        no_deps=True,
                    )
                pending.add(fut)
                if len(pending) >= jobs:
                    done, pending = wait(pending, return_when=FIRST_COMPLETED)
                    for f in done:
                        if f.result() != 0:
                            ex.shutdown(wait=False, cancel_futures=True)
                            sys.exit(f.result())
            while pending:
                done, pending = wait(pending, return_when=FIRST_COMPLETED)
                for f in done:
                    if f.result() != 0:
                        ex.shutdown(wait=False, cancel_futures=True)
                        sys.exit(f.result())

    print(f"[install_all] done -> {out_tp}")


if __name__ == "__main__":
    main()
