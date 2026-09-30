# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Opt-in MapLibre Native CMake build for example binaries only.
# Does not link into product chrome. Requires vcvars + vcpkg vendor pkgs.

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys

VS_ROOT = r"C:\Program Files\Microsoft Visual Studio\18\Community"
CMAKE_CANDIDATES = [
    os.path.join(
        VS_ROOT,
        r"Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    ),
    "cmake",
]
NINJA_CANDIDATES = [
    os.path.join(
        VS_ROOT,
        r"Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    ),
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..",
        "..",
        "build",
        "bin",
        "ninja.exe",
    ),
    "ninja",
]
VCVARS = os.path.join(VS_ROOT, r"VC\Auxiliary\Build\vcvars64.bat")


def find_tool(candidates: list[str]) -> str:
    for path in candidates:
        if path == os.path.basename(path):
            return path
        if os.path.isfile(path):
            return os.path.abspath(path)
    return candidates[-1]


def vcvars_env() -> dict[str, str]:
    env = os.environ.copy()
    if not os.path.isfile(VCVARS):
        return env
    completed = subprocess.run(
        ["cmd", "/c", VCVARS, "&&", "set"],
        check=True,
        capture_output=True,
        text=True,
    )
    for line in completed.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    return env


def run(cmd: list[str], env: dict[str, str], log_path: str | None = None) -> None:
    print(" ".join(cmd), flush=True)
    if log_path:
        os.makedirs(os.path.dirname(log_path), exist_ok=True)
        with open(log_path, "w", encoding="utf-8", errors="replace") as log:
            completed = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
    else:
        completed = subprocess.run(cmd, env=env)
    if completed.returncode != 0:
        if log_path:
            sys.stderr.write(f"build log: {log_path}\n")
            try:
                with open(log_path, encoding="utf-8", errors="replace") as log:
                    tail = log.readlines()[-80:]
                sys.stderr.writelines(tail)
            except OSError:
                pass
        sys.exit(completed.returncode)


def ensure_glfw(vcpkg_root: str, env: dict[str, str]) -> None:
    glfw_lib = os.path.join(vcpkg_root, "installed", "x64-windows", "lib", "glfw3.lib")
    if os.path.isfile(glfw_lib):
        return
    vcpkg_exe = os.path.join(vcpkg_root, "vcpkg.exe")
    if not os.path.isfile(vcpkg_exe):
        bootstrap = os.path.join(vcpkg_root, "bootstrap-vcpkg.bat")
        run(["cmd", "/c", bootstrap], env)
    triplets = os.path.join(
        os.path.dirname(vcpkg_root),
        "vcpkg-custom-triplets",
    )
    run(
        [
            vcpkg_exe,
            "--disable-metrics",
            f"--overlay-triplets={triplets}",
            "--triplet=x64-windows",
            "install",
            "glfw3",
        ],
        env,
    )


def find_built_exe(cmake_dir: str, stem: str) -> str | None:
    candidates = [
        os.path.join(cmake_dir, "bin", f"{stem}.exe"),
        os.path.join(cmake_dir, f"{stem}.exe"),
        os.path.join(cmake_dir, "platform", "glfw", f"{stem}.exe"),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    for root, _dirs, files in os.walk(cmake_dir):
        if f"{stem}.exe" in files:
            return os.path.join(root, f"{stem}.exe")
    return None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("src", help="maplibre-native source root")
    parser.add_argument("out", help="stamp/output directory (binaries land in out/bin)")
    parser.add_argument(
        "config",
        nargs="?",
        default="Release",
        help="Debug or Release (MSVC runtime)",
    )
    parser.add_argument(
        "--cmake-dir",
        default="",
        help="CMake binary dir (default: <repo>/third_party/.build/maplibre-native)",
    )
    parser.add_argument(
        "--with-glfw",
        action="store_true",
        default=True,
        help="Configure and build mbgl-glfw (default on)",
    )
    parser.add_argument(
        "--no-glfw",
        action="store_true",
        help="Skip GLFW example target",
    )
    parser.add_argument(
        "--skip-configure",
        action="store_true",
        help="Skip cmake -S/-B; only build + copy (use after a successful configure)",
    )
    args = parser.parse_args()

    src = os.path.abspath(args.src)
    out = os.path.abspath(args.out)
    with_glfw = args.with_glfw and not args.no_glfw
    config = args.config
    if config not in ("Debug", "Release"):
        sys.stderr.write("config must be Debug or Release\n")
        sys.exit(2)

    repo_third_party = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    cmake_dir = (
        os.path.abspath(args.cmake_dir)
        if args.cmake_dir
        else os.path.join(repo_third_party, ".build", "maplibre-native")
    )
    os.makedirs(cmake_dir, exist_ok=True)
    os.makedirs(os.path.join(out, "bin"), exist_ok=True)

    cmake = find_tool(CMAKE_CANDIDATES)
    ninja = find_tool(NINJA_CANDIDATES)
    env = vcvars_env()
    env["CMAKE_MAKE_PROGRAM"] = ninja
    win_gl_include = os.path.join(src, "platform", "windows", "include")
    # Ensure GLFW GLES3/gl3.h resolves even if CMAKE_*_FLAGS are overwritten.
    existing_include = env.get("INCLUDE", "")
    env["INCLUDE"] = win_gl_include + (
        ";" + existing_include if existing_include else ""
    )

    vcpkg_root = os.path.join(src, "platform", "windows", "vendor", "vcpkg")
    if with_glfw:
        ensure_glfw(vcpkg_root, env)

    runtime = (
        "MultiThreadedDebugDLL" if config == "Debug" else "MultiThreadedDLL"
    )
    # Do not put /I in CMAKE_CXX_FLAGS: it replaces MSVC defaults and breaks
    # STL (#include <map>). GLES headers are on INCLUDE (set above).
    configure = [
        cmake,
        "-S",
        src,
        "-B",
        cmake_dir,
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={config}",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        "-DMLN_WITH_OPENGL=ON",
        "-DMLN_WITH_EGL=OFF",
        "-DMLN_WITH_VULKAN=OFF",
        "-DMLN_WITH_METAL=OFF",
        "-DMLN_WITH_WEBGPU=OFF",
        f"-DMLN_WITH_GLFW={'ON' if with_glfw else 'OFF'}",
        "-DMLN_WITH_WERROR=OFF",
        "-DBUILD_TESTING=OFF",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        f"-DCMAKE_MSVC_RUNTIME_LIBRARY={runtime}",
    ]
    if args.skip_configure:
        print("skip configure", flush=True)
    else:
        run(configure, env)

    targets = ["mbgl-render"]
    if with_glfw:
        targets.append("mbgl-glfw")
    build = [cmake, "--build", cmake_dir, "--target", *targets, "-j"]
    log_path = os.path.join(out, "native-build.log")
    run(build, env, log_path=log_path)

    mapping = {
        "mbgl-render": "maplibre_headless_example.exe",
        "mbgl-glfw": "maplibre_glfw_example.exe",
    }
    for stem, dest_name in mapping.items():
        if stem == "mbgl-glfw" and not with_glfw:
            continue
        built = find_built_exe(cmake_dir, stem)
        if not built:
            sys.stderr.write(f"ERROR: missing built {stem}.exe under {cmake_dir}\n")
            sys.exit(1)
        dest = os.path.join(out, "bin", dest_name)
        shutil.copy2(built, dest)
        # Also place next to product out/ root for easy run.
        root_copy = os.path.join(out, dest_name)
        shutil.copy2(built, root_copy)
        print(f"copied {built} -> {dest}", flush=True)


if __name__ == "__main__":
    main()
