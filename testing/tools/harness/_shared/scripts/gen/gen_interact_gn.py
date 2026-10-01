# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""GN action: generate Interact lexer/parser into out/*/gen (not checked in)."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

_CPP_NAMES = (
    "InteractLexer.cpp",
    "InteractLexer.h",
    "InteractParser.cpp",
    "InteractParser.h",
    "InteractVisitor.cpp",
    "InteractVisitor.h",
    "InteractBaseVisitor.cpp",
    "InteractBaseVisitor.h",
)

_PY_NAMES = (
    "InteractLexer.py",
    "InteractParser.py",
    "InteractVisitor.py",
)


def find_java() -> str:
    env = os.environ.get("JAVA_HOME", "").strip()
    if env:
        cand = Path(env) / "bin" / "java.exe"
        if cand.is_file():
            return str(cand)
        cand = Path(env) / "bin" / "java"
        if cand.is_file():
            return str(cand)
    which = shutil.which("java")
    if which:
        return which
    roots = [
        Path(__file__).resolve().parents[5] / "third_party" / ".tools" / "jdk21",
        Path(r"C:\Program Files\Microsoft"),
        Path(r"C:\Program Files\Eclipse Adoptium"),
        Path(r"C:\Program Files\Java"),
    ]
    for root in roots:
        if not root.exists():
            continue
        for java in root.rglob("java.exe"):
            return str(java)
    raise SystemExit(
        "gen_interact_gn: java not found (set JAVA_HOME or install JDK 21)"
    )


def _collect_flat(src_root: Path, dest: Path, names: tuple[str, ...]) -> None:
    """Move ANTLR outputs to dest root (ANTLR nests by grammar path)."""
    dest.mkdir(parents=True, exist_ok=True)
    for name in names:
        hits = [p for p in src_root.rglob(name) if p.is_file()]
        if not hits:
            raise SystemExit(f"gen_interact_gn: missing {name} under {src_root}")
        shutil.copy2(hits[0], dest / name)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--g4", required=True)
    ap.add_argument("--jar", required=True)
    ap.add_argument("--out-cpp", required=True)
    ap.add_argument("--out-py", required=True)
    args = ap.parse_args()

    g4 = Path(args.g4)
    jar = Path(args.jar)
    out_cpp = Path(args.out_cpp)
    out_py = Path(args.out_py)
    if not g4.is_file():
        raise SystemExit(f"missing g4: {g4}")
    if not jar.is_file():
        raise SystemExit(f"missing antlr jar: {jar}")

    java = find_java()
    # Generate from a flat temp copy so ANTLR does not nest under the repo path.
    with tempfile.TemporaryDirectory(prefix="interact_g4_") as td:
        td_path = Path(td)
        g4_flat = td_path / "Interact.g4"
        shutil.copy2(g4, g4_flat)
        cpp_tmp = td_path / "cpp"
        py_tmp = td_path / "py"
        cpp_tmp.mkdir()
        py_tmp.mkdir()

        cpp_cmd = [
            java,
            "-jar",
            str(jar),
            "-Dlanguage=Cpp",
            "-visitor",
            "-no-listener",
            "-o",
            str(cpp_tmp),
            "-package",
            "interact",
            str(g4_flat),
        ]
        py_cmd = [
            java,
            "-jar",
            str(jar),
            "-Dlanguage=Python3",
            "-visitor",
            "-no-listener",
            "-o",
            str(py_tmp),
            str(g4_flat),
        ]
        subprocess.check_call(cpp_cmd)
        subprocess.check_call(py_cmd)
        _collect_flat(cpp_tmp, out_cpp, _CPP_NAMES)
        _collect_flat(py_tmp, out_py, _PY_NAMES)

    (out_cpp / ".stamp").write_text("ok\n", encoding="utf-8")
    (out_py / ".stamp").write_text("ok\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
