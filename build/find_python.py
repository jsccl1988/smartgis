# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Locate a CPython 3.12 install for in-process embed (headers + import lib + DLL)."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def _gn_escape(value: str) -> str:
    return value.replace("\\", "/").replace('"', '\\"')


def _emit(found: bool, root: str = "", include: str = "", libdir: str = "", dll: str = "") -> None:
    print("found = " + ("true" if found else "false"))
    print('root = "' + _gn_escape(root) + '"')
    print('include = "' + _gn_escape(include) + '"')
    print('libdir = "' + _gn_escape(libdir) + '"')
    print('dll = "' + _gn_escape(dll) + '"')


def _is_embed_root(root: Path) -> bool:
    include = root / "include" / "Python.h"
    if not include.is_file():
        include = root / "Include" / "Python.h"
    lib = root / "libs" / "python312.lib"
    dll = root / "python312.dll"
    return include.is_file() and lib.is_file() and dll.is_file()


def _fields(root: Path) -> tuple[str, str, str, str]:
    include = root / "include"
    if not (include / "Python.h").is_file():
        include = root / "Include"
    return (
        str(root),
        str(include),
        str(root / "libs"),
        str(root / "python312.dll"),
    )


def _from_registry() -> Path | None:
    if os.name != "nt":
        return None
    try:
        import winreg
    except ImportError:
        return None
    keys = [
        (winreg.HKEY_CURRENT_USER, r"SOFTWARE\Python\PythonCore\3.12\InstallPath"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Python\PythonCore\3.12\InstallPath"),
        (winreg.HKEY_CURRENT_USER, r"SOFTWARE\Python\PythonCore\3.12-64\InstallPath"),
        (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Python\PythonCore\3.12-64\InstallPath"),
    ]
    for hive, path in keys:
        try:
            with winreg.OpenKey(hive, path) as key:
                value, _ = winreg.QueryValueEx(key, "")
        except OSError:
            continue
        candidate = Path(value)
        if _is_embed_root(candidate):
            return candidate
    return None


def _from_py_launcher() -> Path | None:
    try:
        out = subprocess.check_output(
            ["py", "-3.12", "-c", "import sys; print(sys.base_prefix)"],
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    candidate = Path(out.strip())
    if _is_embed_root(candidate):
        return candidate
    return None


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    candidates: list[Path] = []

    env = os.environ.get("SMT_PYTHON_ROOT", "").strip()
    if env:
        candidates.append(Path(env))

    candidates.append(repo / "third_party" / "python")
    local = os.environ.get("LOCALAPPDATA", "")
    if local:
        candidates.append(Path(local) / "Programs" / "Python" / "Python312")
    candidates.append(Path(r"C:\Python312"))
    candidates.append(Path(r"C:\Program Files\Python312"))

    for raw in candidates:
        try:
            root = raw.resolve()
        except OSError:
            continue
        if _is_embed_root(root):
            _emit(True, *_fields(root))
            return 0

    reg = _from_registry()
    if reg is not None:
        _emit(True, *_fields(reg))
        return 0

    launched = _from_py_launcher()
    if launched is not None:
        _emit(True, *_fields(launched))
        return 0

    _emit(False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
