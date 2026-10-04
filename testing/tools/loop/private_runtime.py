# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Private SmartGIS runtime copy for multi-agent harness races.

Linker/PE races (0xC0000135 STATUS_DLL_NOT_FOUND, WinError 32) happen when
agents rebuild ``SmartGIS.exe`` / sibling DLLs while another process
loads the live path. Review / scenic matrix rows prefer a same-dir alias
``out/<config>/SmartGIS_<tag>.exe`` so ``exe_capture_path`` still writes
under ``out/<config>/captures/`` (not ``out/scratch/.../captures``). A scratch
copy is kept as fallback when the gen-root alias cannot be written.
"""

from __future__ import annotations

import os
import shutil
import time
from pathlib import Path

# DLLs that scenic / content present commonly need at process start.
_CORE_DLLS = (
    "scenic.dll",
    "scenic_d.dll",
    "scenic_impl.dll",
    "scenic_impl_d.dll",
    "content.dll",
    "content_d.dll",
    "base.dll",
    "base_d.dll",
    "gis.dll",
    "gis_d.dll",
    "ui_views.dll",
    "ui_gfx.dll",
    "render.dll",
    "render_d.dll",
    "gpu.dll",
    "vista.dll",
    "vista_d.dll",
    "effect.dll",
    "tool.dll",
    "plugin.dll",
    "net.dll",
)


def wait_readable(path: Path, *, timeout_sec: float = 90.0) -> bool:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if path.is_file():
            try:
                with open(path, "rb"):
                    return True
            except OSError:
                pass
        time.sleep(0.75)
    return False


def acquire_run_lock(lock_path: Path, *, timeout_sec: float = 180.0) -> bool:
    """Best-effort exclusive lock file (Windows create-exclusive)."""
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        try:
            fd = os.open(str(lock_path), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, str(os.getpid()).encode("ascii", errors="ignore"))
            os.close(fd)
            return True
        except FileExistsError:
            # Stale lock: drop if holder PID is gone.
            try:
                text = lock_path.read_text(encoding="utf-8", errors="ignore").strip()
                pid = int(text) if text.isdigit() else 0
            except OSError:
                pid = 0
            if pid > 0:
                try:
                    os.kill(pid, 0)
                except OSError:
                    try:
                        lock_path.unlink(missing_ok=True)  # type: ignore[call-arg]
                    except TypeError:
                        if lock_path.is_file():
                            lock_path.unlink()
                    except OSError:
                        pass
            time.sleep(1.0)
        except OSError:
            time.sleep(1.0)
    return False


def release_run_lock(lock_path: Path) -> None:
    try:
        lock_path.unlink(missing_ok=True)  # type: ignore[call-arg]
    except TypeError:
        if lock_path.is_file():
            try:
                lock_path.unlink()
            except OSError:
                pass
    except OSError:
        pass


def prepare_private_views_exe(
    out_dir: Path,
    *,
    tag: str = "scenic_review",
    exe_name: str = "SmartGIS.exe",
    timeout_sec: float = 90.0,
    copy_dlls: bool = True,
) -> Path | None:
    """Copy ``exe_name`` to a private path that avoids locking the live PE.

    Prefer ``out_dir/SmartGIS_<tag>.exe`` (same directory → captures stay
    under the gen root). Fall back to ``out/scratch/<tag>/SmartGIS.exe``.
    """
    src = out_dir / exe_name
    if not wait_readable(src, timeout_sec=timeout_sec):
        return None

    scratch = out_dir.parent / "scratch" / tag
    scratch.mkdir(parents=True, exist_ok=True)
    scratch_exe = scratch / exe_name
    # Same-dir alias keeps exe_capture_path under out/<config>/captures/.
    alias = out_dir / f"SmartGIS_{tag}.exe"

    for attempt in range(8):
        alias_ok = False
        scratch_ok = False
        try:
            shutil.copy2(src, alias)
            alias_ok = wait_readable(alias, timeout_sec=15.0)
        except OSError:
            alias_ok = False
        try:
            shutil.copy2(src, scratch_exe)
            scratch_ok = wait_readable(scratch_exe, timeout_sec=15.0)
        except OSError:
            scratch_ok = False
        if copy_dlls and scratch_ok:
            for name in _CORE_DLLS:
                dll = out_dir / name
                if not dll.is_file():
                    continue
                if not wait_readable(dll, timeout_sec=15.0):
                    continue
                try:
                    shutil.copy2(dll, scratch / name)
                except OSError:
                    pass
        if alias_ok:
            return alias
        if scratch_ok:
            return scratch_exe
        time.sleep(1.0 + attempt * 0.5)
    return None


def with_private_path(env: dict[str, str], out_dir: Path, private_exe: Path) -> dict[str, str]:
    """Point PATH at gen-root (and scratch if used); set SMARTGIS_HARNESS_EXE."""
    out = env.copy()
    prior = out.get("PATH", "")
    private_dir = private_exe.parent
    parts = [str(out_dir)]
    if private_dir.resolve() != out_dir.resolve():
        parts.insert(0, str(private_dir))
    out["PATH"] = os.pathsep.join(parts) + (os.pathsep + prior if prior else "")
    out["SMARTGIS_HARNESS_EXE"] = str(private_exe)
    return out
