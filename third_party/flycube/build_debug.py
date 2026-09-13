# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Configure and build a Debug /MDd FlyCube.lib from the local junction.
# Output stays under smartgis out/flycube — never overwrite the Release prebuilt.

import os
import subprocess
import sys

VS_ROOT = r"C:\Program Files\Microsoft Visual Studio\18\Community"
CMAKE_CANDIDATES = [
    os.path.join(VS_ROOT,
                 r"Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"),
    "cmake",
]
NINJA_CANDIDATES = [
    os.path.join(VS_ROOT,
                 r"Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"),
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                 "build", "bin", "ninja.exe"),
    "ninja",
]
VCVARS = os.path.join(VS_ROOT, r"VC\Auxiliary\Build\vcvars64.bat")


def find_tool(candidates):
    for path in candidates:
        if path == os.path.basename(path):
            return path
        if os.path.isfile(path):
            return os.path.abspath(path)
    return candidates[-1]


def vcvars_env():
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


def run(cmd, env):
    print(" ".join(cmd), flush=True)
    completed = subprocess.run(cmd, env=env)
    if completed.returncode != 0:
        sys.exit(completed.returncode)


def main():
    if len(sys.argv) < 3:
        sys.stderr.write("usage: build_debug.py <flycube_src> <build_dir>\n")
        sys.exit(2)
    src = os.path.abspath(sys.argv[1])
    dest = os.path.abspath(sys.argv[2])
    os.makedirs(dest, exist_ok=True)

    cmake = find_tool(CMAKE_CANDIDATES)
    ninja = find_tool(NINJA_CANDIDATES)
    env = vcvars_env()
    env["CMAKE_MAKE_PROGRAM"] = ninja
    cl = env.get("CXX", env.get("CC", "cl.exe"))

    configure = [
        cmake,
        "-S",
        src,
        "-B",
        dest,
        "-G",
        "Ninja",
        "--preset",
        "default",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_MAKE_PROGRAM=" + ninja,
        "-DCMAKE_C_COMPILER=" + cl,
        "-DCMAKE_CXX_COMPILER=" + cl,
        "-DBUILD_SAMPLES=OFF",
        "-DBUILD_TESTING=OFF",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL",
    ]
    build = [cmake, "--build", dest, "--target", "FlyCube", "-j"]
    run(configure, env)
    run(build, env)

    lib_path = os.path.join(dest, "lib", "FlyCube.lib")
    if not os.path.isfile(lib_path):
        sys.stderr.write("ERROR: missing %s\n" % lib_path)
        sys.exit(1)


if __name__ == "__main__":
    main()
