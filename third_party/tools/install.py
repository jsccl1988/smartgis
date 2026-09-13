#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""
Install one manifest package (CMake or Makefile) into a prefix.
默认按 manifest（install_requires / cmake_prefix_paths）与 CMake 扫描（find_package 等）合并后的顺序安装传递依赖。
GN action / batch.py 批量调用时请传 --no-deps（顺序已由上层保证）。
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

from cmake_deps import package_src_dir
from deps import topo_install_order
from fetch import fetch_one


def _run(cmd: list[str], cwd: Path, env: dict | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    r = subprocess.run(cmd, cwd=cwd, env=env, check=False)
    if r.returncode != 0:
        sys.exit(r.returncode)


def default_cmake_generator() -> str:
    """Prefer Ninja when available to match GN tp_cmake_generator (tp.gni).

    On Windows, Visual Studio is the default (MSVC). Override with
    MGIS_CMAKE_GENERATOR. Ninja on win32 still needs a VS developer prompt.
    """
    override = (os.environ.get("MGIS_CMAKE_GENERATOR") or "").strip()
    if override:
        return override
    if sys.platform == "win32":
        if Path(r"C:\Program Files\Microsoft Visual Studio\18").is_dir():
            return "Visual Studio 18 2026"
        if Path(r"C:\Program Files\Microsoft Visual Studio\2022").is_dir():
            return "Visual Studio 17 2022"
        return "Visual Studio 17 2022"
    if shutil.which("ninja"):
        return "Ninja"
    return "Unix Makefiles"


def default_c_compiler() -> str:
    """Default C compiler. Unused on Windows (MSVC via -G / -A)."""
    if sys.platform == "win32":
        return None
    gcc12 = "/opt/compiler/gcc-12/bin/gcc"
    if Path(gcc12).exists():
        return gcc12
    if shutil.which("gcc"):
        return "gcc"
    return None


def default_cxx_compiler() -> str:
    """Default C++ compiler. Unused on Windows (MSVC via -G / -A)."""
    if sys.platform == "win32":
        return None
    gcc12 = "/opt/compiler/gcc-12/bin/g++"
    if Path(gcc12).exists():
        return gcc12
    if shutil.which("g++"):
        return "g++"
    return None


def _msvc_runtime_library(build_type: str) -> str:
    """Match mgis BUILDCONFIG static_link_crt=false (MultiThreaded[Debug]DLL)."""
    debug = (build_type or "").lower() in ("debug",)
    static = (os.environ.get("MGIS_STATIC_CRT") or "").strip() in ("1", "true", "yes")
    name = "MultiThreaded"
    if debug:
        name += "Debug"
    if not static:
        name += "DLL"
    return name


def _is_multi_config_generator(generator: str) -> bool:
    g = generator or ""
    return "Visual Studio" in g or g == "Xcode" or "Multi-Config" in g


def _load_manifest(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def _find_package(manifest: dict, name: str) -> dict:
    for pkg in manifest.get("packages") or []:
        if pkg.get("name") == name:
            return pkg
    print(f"[install] ERROR: unknown package {name!r}", file=sys.stderr)
    sys.exit(1)


def _resolve_internal_package_name(pkg_name: str) -> str:
    """将 manifest 包名映射到内部环境的实际目录名。"""
    _INTERNAL_PKG_MAP = {
        "brpc": "baidu-rpc",
        "fmt": "fmtlib",
        "benchmark": "google-benchmark",
    }
    return _INTERNAL_PKG_MAP.get(pkg_name, pkg_name)


# Bundled protobuf from prebuilts (e.g. libtorch) must not replace tree 3.6.1.
_PREBUILT_SKIP_LIB_PREFIXES = ("libprotobuf", "libprotoc")
_PREBUILT_SKIP_INCLUDE_DIRS = frozenset({"google"})


def _is_prebuilt_package(pkg_dir: Path) -> bool:
    """True when pkg_dir looks like a prebuilt prefix (include/ + lib/)."""
    return (pkg_dir / "include").is_dir() and (pkg_dir / "lib").is_dir()


def _is_prebuilt_install(pkg: dict, pkg_dir: Path, *, found_outside_src: bool) -> bool:
    """Manifest ``build_system: prebuilt``, or a foreign prefix that looks prebuilt."""
    if (pkg.get("build_system") or "").lower() == "prebuilt":
        return True
    return found_outside_src and _is_prebuilt_package(pkg_dir)


def _should_skip_prebuilt_lib(name: str) -> bool:
    return name.startswith(_PREBUILT_SKIP_LIB_PREFIXES)


def _merge_copy_tree(
    src: Path,
    dst: Path,
    *,
    skip_dir_names: frozenset[str] = frozenset(),
) -> None:
    """Merge src into dst. Never rmtree the destination tree."""
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        if item.name in skip_dir_names:
            print(
                f"[install] skip prebuilt {item.name} (protect sibling prefix)",
                flush=True,
            )
            continue
        dest = dst / item.name
        if item.is_dir():
            shutil.copytree(item, dest, dirs_exist_ok=True)
        else:
            shutil.copy2(item, dest)


def _install_prebuilt_pkg(pkg_dir: Path, install_prefix: Path) -> None:
    """Merge-copy a prebuilt prefix into the unified install tree."""
    print(f"[install] installing prebuilt package from {pkg_dir}", flush=True)
    install_prefix.mkdir(parents=True, exist_ok=True)
    src_include = pkg_dir / "include"
    if src_include.is_dir():
        dst_include = install_prefix / "include"
        _merge_copy_tree(
            src_include, dst_include, skip_dir_names=_PREBUILT_SKIP_INCLUDE_DIRS
        )
        print(f"[install] merged include -> {dst_include}", flush=True)
    src_lib = pkg_dir / "lib"
    if src_lib.is_dir():
        dst_lib = install_prefix / "lib"
        dst_lib.mkdir(parents=True, exist_ok=True)
        for item in src_lib.iterdir():
            if _should_skip_prebuilt_lib(item.name):
                print(f"[install] skip bundled {item.name}", flush=True)
                continue
            dest = dst_lib / item.name
            if item.is_dir():
                shutil.copytree(item, dest, dirs_exist_ok=True)
            else:
                shutil.copy2(item, dest)
        print(f"[install] merged lib -> {dst_lib}", flush=True)
    for extra in ("bin", "share"):
        src_extra = pkg_dir / extra
        if src_extra.is_dir():
            dst_extra = install_prefix / extra
            _merge_copy_tree(src_extra, dst_extra)
            print(f"[install] merged {extra} -> {dst_extra}", flush=True)
    _relocate_shadowing_bssl_openssl(install_prefix)


def _relocate_shadowing_bssl_openssl(install_prefix: Path) -> None:
    """If unified include/openssl is BoringSSL, move it so brpc can use system OpenSSL.

    grpc (and some prebuilts) install BoringSSL headers as include/openssl/, which
    shadows /usr/include/openssl when CMAKE_PREFIX_PATH points at the unified tree.
    """
    openssl_inc = install_prefix / "include" / "openssl"
    if not openssl_inc.is_dir():
        return
    marker = openssl_inc / "base.h"
    if not marker.is_file():
        return
    try:
        text = marker.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return
    if "bssl" not in text and "BORINGSSL" not in text:
        return
    dest_parent = install_prefix / "include" / "bssl"
    dest = dest_parent / "openssl"
    dest_parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        shutil.rmtree(dest)
    print(
        f"[install] relocating BoringSSL headers {openssl_inc} -> {dest} "
        "(avoid shadowing system OpenSSL for brpc)",
        flush=True,
    )
    shutil.move(str(openssl_inc), str(dest))


def _paths_for_package(
    *,
    package_name: str,
    build_root: Path,
    install_prefix: Path,
    unified_prefix: bool,
) -> tuple[Path, Path, Path]:
    """返回 (build_dir, install_prefix_for_pkg, stamp_path)。"""
    if unified_prefix:
        build_parent = build_root.parent
        bdir = build_parent / package_name
        stamp = bdir / ".gn_publish_stamp"
        return bdir, install_prefix, stamp
    tp_root = install_prefix.parent
    dinst = tp_root / package_name
    bdir = tp_root / "build" / package_name
    stamp = dinst / ".gn_publish_stamp"
    return bdir, dinst, stamp


def _install_py_path() -> Path:
    return Path(__file__).resolve()


# Relative markers under a unified install prefix that mean "already present"
# (e.g. after mv from out/third_party). Avoids rebuild/fetch when calibrate
# pulls in deps that the prefix already satisfies.
_PREFIX_MARKERS: dict[str, tuple[str, ...]] = {
    "abseil-cpp": ("include/absl/base/config.h", "lib/libabsl_base.a"),
    "gflags": ("include/gflags/gflags.h", "lib/libgflags.a"),
    "glog": (
        "include/glog/logging.h",
        "lib/libglog.a",
        "lib/glog.lib",
        "lib/glogd.lib",
    ),
    "protobuf": (
        "include/google/protobuf/message.h",
        "lib/libprotobuf.a",
    ),
    "leveldb": ("include/leveldb/db.h", "lib/libleveldb.a"),
    # Require the archive: headers may exist without a successful linkable install.
    "brpc": ("lib/libbrpc.a",),
    "grpc": ("lib/libgrpc++.a",),
    "yaml-cpp": ("include/yaml-cpp/yaml.h", "lib/libyaml-cpp.a"),
    "rocksdb": ("include/rocksdb/db.h", "lib/librocksdb.a"),
    "sqlite3": ("include/sqlite3.h", "lib/sqlite3.lib", "lib/libsqlite3.a"),
    "geos": ("include/geos_c.h", "lib/geos.lib", "lib/libgeos.a"),
    "PROJ": ("include/proj.h", "lib/proj.lib", "lib/proj_d.lib", "lib/libproj.a"),
    "gdal": ("include/gdal.h", "lib/gdal.lib", "lib/gdald.lib", "lib/libgdal.a"),
    "SQLiteCpp": ("include/SQLiteCpp/SQLiteCpp.h", "lib/SQLiteCpp.lib"),
    "googletest": ("include/gtest/gtest.h", "lib/gtest.lib", "lib/gtestd.lib"),
    "benchmark": ("include/benchmark/benchmark.h", "lib/benchmark.lib"),
    "cpp-httplib": ("include/httplib.h", "include/cpp-httplib/httplib.h"),
}


def _prefix_has_package(install_prefix: Path, package_name: str) -> bool:
    """True if unified prefix already has a recognizable artifact for package_name."""
    markers = _PREFIX_MARKERS.get(package_name)
    if not markers:
        return False
    return any((install_prefix / rel).is_file() for rel in markers)


def _ensure_fetched(
    *,
    pkg: dict,
    src_root: Path,
    manifest_path: Path,
) -> Path:
    """Ensure package source exists under src_root; fetch from manifest if missing."""
    src = package_src_dir(src_root, pkg)
    if src.is_dir() and any(src.iterdir()):
        return src
    if not (pkg.get("git_url") or pkg.get("http_url")):
        print(
            f"[install] ERROR: missing source {src} for {pkg.get('name')!r} "
            "and manifest has no git_url/http_url to fetch",
            file=sys.stderr,
        )
        sys.exit(1)
    tp_root = manifest_path.parent
    print(
        f"[install] source missing for {pkg.get('name')!r}; fetching into {src_root}",
        flush=True,
    )
    fetch_one(tp_root, src_root, pkg, shallow=True)
    src = package_src_dir(src_root, pkg)
    if not src.is_dir():
        print(
            f"[install] ERROR: fetch completed but source still missing: {src}",
            file=sys.stderr,
        )
        sys.exit(1)
    return src


def _ensure_deps(
    args: argparse.Namespace,
    manifest: dict,
    order: list[str],
) -> None:
    """安装 order 中除最后一个（主包）以外的依赖。"""
    install_py = _install_py_path()
    for name in order[:-1]:
        dep_build, dep_install, dep_stamp = _paths_for_package(
            package_name=name,
            build_root=args.build_root,
            install_prefix=args.install_prefix,
            unified_prefix=args.unified_prefix,
        )
        if dep_stamp.is_file():
            print(f"[install] dep {name}: skip (stamp {dep_stamp})", flush=True)
            continue
        if args.unified_prefix and _prefix_has_package(dep_install, name):
            dep_stamp.parent.mkdir(parents=True, exist_ok=True)
            dep_stamp.write_text(
                f"skip {name}: already in prefix {dep_install}\n",
                encoding="utf-8",
            )
            print(
                f"[install] dep {name}: skip (already in prefix {dep_install})",
                flush=True,
            )
            continue
        dep_build.parent.mkdir(parents=True, exist_ok=True)
        dep_install.mkdir(parents=True, exist_ok=True)
        cmd = [
            sys.executable,
            str(install_py),
            "--manifest",
            str(args.manifest.resolve()),
            "--package",
            name,
            "--src-root",
            str(args.src_root.resolve()),
            "--build-root",
            str(dep_build.resolve()),
            "--install-prefix",
            str(dep_install.resolve()),
            "--stamp",
            str(dep_stamp.resolve()),
            "--generator",
            args.generator,
            "--build-type",
            args.build_type,
            "--no-deps",
        ]
        if args.unified_prefix:
            cmd.append("--unified-prefix")
        if args.no_cmake_calibrate:
            cmd.append("--no-cmake-calibrate")
        print("+", " ".join(cmd), flush=True)
        tp_cwd = args.manifest.parent
        r = subprocess.run(cmd, cwd=tp_cwd, check=False)
        if r.returncode != 0:
            sys.exit(r.returncode)


def _clear_stale_cmake_cache_if_needed(build_dir: Path, generator: str) -> None:
    """Drop CMakeCache/CMakeFiles when the tree was configured with another -G."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return
    prev: str | None = None
    try:
        text = cache.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return
    for line in text.splitlines():
        if line.startswith("CMAKE_GENERATOR:INTERNAL="):
            prev = line.split("=", 1)[1].strip()
            break
    if prev is None or prev == generator:
        return
    print(
        f"[install] CMake generator mismatch (cache={prev!r}, requested={generator!r}); "
        "removing CMakeCache.txt and CMakeFiles/",
        flush=True,
    )
    try:
        cache.unlink()
    except OSError:
        pass
    cmake_files = build_dir / "CMakeFiles"
    if cmake_files.is_dir():
        shutil.rmtree(cmake_files, ignore_errors=True)


def build_cmake(
    pkg: dict,
    src_dir: Path,
    build_dir: Path,
    install_prefix: Path,
    generator: str,
    build_type: str,
    *,
    unified_prefix: bool = False,
) -> None:
    src_dir = src_dir.resolve()
    build_dir = build_dir.resolve()
    install_prefix = install_prefix.resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    _clear_stale_cmake_cache_if_needed(build_dir, generator)
    cmake_args = [
        "cmake",
        "-S",
        str(src_dir),
        "-B",
        str(build_dir),
        "-G",
        generator,
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DCMAKE_INSTALL_PREFIX={install_prefix}",
    ]
    if sys.platform == "win32":
        if "Visual Studio" in generator:
            cmake_args.extend(["-A", "x64"])
        cmake_args.extend(
            [
                "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
                "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                f"-DCMAKE_MSVC_RUNTIME_LIBRARY={_msvc_runtime_library(build_type)}",
            ]
        )
        winver = (os.environ.get("MGIS_WINVER") or "0x0A00").strip()
        cmake_args.append(
            f'-DCMAKE_CXX_FLAGS_INIT=/D_WIN32_WINNT={winver} /wd4819'
        )
        cmake_args.append(
            f'-DCMAKE_C_FLAGS_INIT=/D_WIN32_WINNT={winver} /wd4819'
        )
    else:
        cc = default_c_compiler()
        cxx = default_cxx_compiler()
        if cc:
            cmake_args.append(f"-DCMAKE_C_COMPILER={cc}")
            print(f"[install] using C compiler: {cc}", flush=True)
        if cxx:
            cmake_args.append(f"-DCMAKE_CXX_COMPILER={cxx}")
            print(f"[install] using CXX compiler: {cxx}", flush=True)
    extra = pkg.get("cmake_args") or []
    cmake_args.extend(
        str(x).replace("@PREFIX@", str(install_prefix)) for x in extra
    )
    if pkg.get("cmake_fetch_root_arg") and "cmake_from_tp_root" in pkg:
        fetch_root = pkg.get("cmake_fetch_root") or "scann"
        cmake_args.append(
            f"-D{pkg['cmake_fetch_root_arg']}={src_base / fetch_root}"
        )
    prefix_names = pkg.get("cmake_prefix_paths")
    if prefix_names:
        if unified_prefix:
            paths = [str(install_prefix.resolve())]
        else:
            tp_root = install_prefix.parent
            paths = [str((tp_root / p).resolve()) for p in prefix_names]
        cmake_args.append(f"-DCMAKE_PREFIX_PATH={os.pathsep.join(paths)}")
    if pkg.get("cmake_toolchain_file"):
        cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={pkg['cmake_toolchain_file']}")

    pre = pkg.get("pre_script")
    if pre:
        tp_root = Path(__file__).resolve().parent.parent
        script = Path(pre)
        if not script.is_absolute():
            script = (tp_root / script).resolve()
        if not script.is_file():
            print(
                f"[install] ERROR: pre_script not found: {script}",
                file=sys.stderr,
            )
            sys.exit(1)
        _run(
            ["bash", str(script), str(src_dir), str(build_dir), str(install_prefix)],
            cwd=src_dir,
        )

    _run(cmake_args, cwd=build_dir.parent)
    j = os.environ.get("INCUBATOR_BUILD_JOBS", str(os.cpu_count() or 4))
    config = "Debug" if (build_type or "").lower() == "debug" else "Release"
    if _is_multi_config_generator(generator):
        _run(
            [
                "cmake",
                "--build",
                str(build_dir),
                "--config",
                config,
                "--parallel",
                j,
            ],
            cwd=build_dir.parent,
        )
        _run(
            [
                "cmake",
                "--install",
                str(build_dir),
                "--config",
                config,
                "--prefix",
                str(install_prefix),
            ],
            cwd=build_dir.parent,
        )
    elif "Ninja" in generator:
        _run(["ninja", "-C", str(build_dir)], cwd=build_dir.parent)
        _run(["cmake", "--install", str(build_dir)], cwd=build_dir.parent)
    else:
        _run(
            ["cmake", "--build", str(build_dir), "--parallel", j],
            cwd=build_dir.parent,
        )
        _run(["cmake", "--install", str(build_dir)], cwd=build_dir.parent)
    _relocate_shadowing_bssl_openssl(install_prefix)


def build_makefile(pkg: dict, src_dir: Path, install_prefix: Path) -> None:
    env = os.environ.copy()
    env.setdefault("CFLAGS", "-fPIC")
    env.setdefault("CXXFLAGS", "-fPIC")
    make = pkg.get("make_bin") or "make"
    target = pkg.get("make_target", "all")
    extra = pkg.get("make_args") or []
    _run([make, *extra, target], cwd=src_dir, env=env)
    prefix_var = pkg.get("make_install_prefix_var") or "PREFIX"
    custom = pkg.get("make_install_args")
    if custom:
        expanded = [a.replace("@PREFIX@", str(install_prefix)) for a in custom]
        _run([make, *expanded], cwd=src_dir, env=env)
    else:
        _run([make, f"{prefix_var}={install_prefix}", "install"], cwd=src_dir, env=env)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--src-root", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--install-prefix", type=Path, required=True)
    parser.add_argument("--stamp", type=Path, required=True)
    parser.add_argument(
        "--generator",
        default=default_cmake_generator(),
        help="CMake -G (default: Ninja if on PATH, else Unix Makefiles)",
    )
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--clean-install", action="store_true")
    parser.add_argument(
        "--unified-prefix",
        action="store_true",
        help="cmake_prefix_paths 指向同一 CMAKE_INSTALL_PREFIX（合并安装树）",
    )
    parser.add_argument(
        "--no-deps",
        action="store_true",
        help="不解析/安装传递依赖（batch 或已手动装齐依赖时使用）",
    )
    parser.add_argument(
        "--no-cmake-calibrate",
        action="store_true",
        help="禁用对源码 CMakeLists.txt 的依赖扫描，仅使用 manifest",
    )
    args = parser.parse_args()
    args.manifest = args.manifest.resolve()
    args.src_root = args.src_root.resolve()
    args.build_root = args.build_root.resolve()
    args.install_prefix = args.install_prefix.resolve()
    args.stamp = args.stamp.resolve()

    manifest = _load_manifest(args.manifest)
    pkg = _find_package(manifest, args.package)
    if pkg.get("install_skip"):
        print(
            f"[install] skip {args.package!r} (manifest install_skip); fetch only",
            flush=True,
        )
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text(f"skip {args.package}\n", encoding="utf-8")
        sys.exit(0)

    if not args.no_deps:
        try:
            order = topo_install_order(
                args.package,
                manifest,
                src_root=args.src_root,
                cmake_calibrate=not args.no_cmake_calibrate,
            )
        except ValueError as e:
            print(f"[install] ERROR: {e}", file=sys.stderr)
            sys.exit(1)
        if len(order) > 1:
            print(f"[install] dependency order: {' -> '.join(order)}", flush=True)
        _ensure_deps(args, manifest, order)

    expected_local = package_src_dir(args.src_root, pkg)
    src_base = expected_local
    # If local tree is missing, fetch from manifest before prebuilt search / cmake.
    if not src_base.is_dir() or not any(src_base.iterdir()):
        if pkg.get("git_url") or pkg.get("http_url"):
            src_base = _ensure_fetched(
                pkg=pkg,
                src_root=args.src_root,
                manifest_path=args.manifest,
            )
            expected_local = src_base
    # 如果本地不存在，尝试从预编译产物目录查找
    # 查找顺序：third-64 -> thirdsrc -> 内部环境（环境变量）
    prebuilt_roots = []

    # 从环境变量获取预编译产物目录（多个目录，用冒号分隔）
    prebuilt_env = os.environ.get("INCUBATOR_TP_PREBUILT_ROOTS", "")
    if prebuilt_env:
        for root in prebuilt_env.split(":"):
            if Path(root).is_dir():
                prebuilt_roots.append(Path(root))

    # 如果环境变量未设置，使用默认的相对路径
    if not prebuilt_roots:
        # 默认相对于主工程根目录的预编译产物目录
        # 假设当前项目在 /home/baidu/master/baidu/personal-code/ 下
        master_root = args.src_root.parent.parent.parent.parent  # /home/baidu/master
        third_64 = master_root / "third-64"
        thirdsrc = master_root / "thirdsrc"
        if third_64.is_dir():
            prebuilt_roots.append(third_64)
        if thirdsrc.is_dir():
            prebuilt_roots.append(thirdsrc)

    # 添加内部环境源码目录作为最后选项
    internal_root = os.environ.get("INCUBATOR_TP_INTERNAL_ROOT")
    if internal_root:
        for root in internal_root.split(":"):
            if Path(root).is_dir():
                prebuilt_roots.append(Path(root))

    if not src_base.is_dir() and prebuilt_roots:
        # 在预编译产物目录中查找
        candidates = []
        for cand in (
            _resolve_internal_package_name(args.package),
            pkg.get("src_dir_name") or "",
            args.package,
        ):
            if cand and cand not in candidates:
                candidates.append(cand)
        for root in prebuilt_roots:
            hit = None
            for cand in candidates:
                internal_path = root / cand
                if internal_path.is_dir():
                    hit = internal_path
                    break
            if hit is None:
                continue
            internal_ref = pkg.get("internal_ref", "")
            internal_ver = pkg.get("internal_version", "unknown")
            internal_note = pkg.get("internal_note", "")
            print(f"[install] using prebuilt package: {hit}", flush=True)
            if internal_ref:
                print(
                    f"[install]   internal version: {internal_ver} ({internal_ref})",
                    flush=True,
                )
                if internal_note:
                    print(f"[install]   note: {internal_note}", flush=True)
            src_base = hit
            break

    sub = pkg.get("cmake_source_subdir")
    cmake_from_tp = pkg.get("cmake_from_tp_root")
    found_outside_src = src_base.resolve() != expected_local.resolve()
    is_prebuilt = _is_prebuilt_install(
        pkg, src_base, found_outside_src=found_outside_src
    )
    if cmake_from_tp:
        tp_root = args.manifest.parent
        src_dir = tp_root / cmake_from_tp
        if not src_dir.is_dir():
            print(
                f"[install] ERROR: missing cmake_from_tp_root {src_dir} (package {args.package!r})",
                file=sys.stderr,
            )
            sys.exit(1)
    elif sub and not sub in src_base.name and not is_prebuilt:
        src_dir = src_base / sub
        if not src_dir.is_dir():
            print(
                f"[install] ERROR: missing cmake_source_subdir {src_dir} (package {args.package!r})",
                file=sys.stderr,
            )
            sys.exit(1)
    elif sub:
        src_dir = src_base / sub
    else:
        src_dir = src_base

    system = (pkg.get("build_system") or "cmake").lower()
    if system == "prebuilt":
        is_prebuilt = True
    args.install_prefix.mkdir(parents=True, exist_ok=True)

    if is_prebuilt:
        if not src_base.is_dir():
            print(
                f"[install] ERROR: missing prebuilt/source tree {src_base} "
                f"(package {args.package!r})",
                file=sys.stderr,
            )
            sys.exit(1)
        _install_prebuilt_pkg(src_dir, args.install_prefix)
    else:
        # 正常构建流程，显示上游版本信息
        git_ref = pkg.get("git_ref", "unknown")
        git_url = pkg.get("git_url", "")
        print(f"[install] building from source: {args.package} (upstream {git_ref})", flush=True)
        if git_url:
            print(f"[install]   url: {git_url}", flush=True)
        if args.clean_install:
            if args.unified_prefix:
                if args.build_root.exists():
                    shutil.rmtree(args.build_root)
            elif args.install_prefix.exists():
                for child in args.install_prefix.iterdir():
                    if child.name in (".gn_publish_stamp",):
                        continue
                    if child.is_dir():
                        shutil.rmtree(child)
                    else:
                        child.unlink()

        if system == "cmake":
            build_cmake(
                pkg,
                src_dir=src_dir,
                build_dir=args.build_root,
                install_prefix=args.install_prefix,
                generator=args.generator,
                build_type=args.build_type,
                unified_prefix=args.unified_prefix,
            )
        elif system == "makefile":
            build_makefile(pkg, src_dir=src_dir, install_prefix=args.install_prefix)
        else:
            print(f"[install] ERROR: unsupported build_system {system!r}", file=sys.stderr)
            sys.exit(1)

    args.stamp.parent.mkdir(parents=True, exist_ok=True)
    args.stamp.write_text(f"ok {args.package}\n", encoding="utf-8")
    print(f"[install] {args.package} -> {args.install_prefix}")


if __name__ == "__main__":
    main()
