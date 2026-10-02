# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Execute a single OS-inject step dict against an HWND."""

from __future__ import annotations

import ctypes
import time
from ctypes import wintypes
from typing import Any

from .input_api import (
    MK_LBUTTON,
    MOUSEEVENTF_MOVE,
    MOUSEEVENTF_WHEEL,
    WM_COMMAND,
    WM_KEYDOWN,
    WM_KEYUP,
    WM_LBUTTONDBLCLK,
    WM_LBUTTONUP,
    WM_MOUSEWHEEL,
    _lparam,
    _mod_vk,
    _post_click,
    _post_pan,
    _post_path,
    _send_click,
    _send_key,
    _send_mouse_abs,
    _send_pan,
    _send_path,
    _step_allows_os,
    _vk,
    _client_to_screen,
    user32,
)

def _run_step(
    step: dict[str, Any],
    hwnd: int,
    *,
    inject_default: str,
    report: dict[str, Any],
    shell_hwnd: int = 0,
) -> None:
    if not _step_allows_os(step):
        report["steps_skipped"] += 1
        return
    op = step.get("op")
    inject = str(step.get("inject") or inject_default).lower()
    shell = int(shell_hwnd) if shell_hwnd else int(hwnd)

    if op == "seq":
        for child in step.get("steps") or []:
            if isinstance(child, dict):
                _run_step(
                    child,
                    hwnd,
                    inject_default=inject_default,
                    report=report,
                    shell_hwnd=shell,
                )
        report["steps_run"] += 1
        return
    if op == "repeat":
        count = max(1, int(step.get("count", 1)))
        for _ in range(count):
            for child in step.get("steps") or []:
                if isinstance(child, dict):
                    _run_step(
                        child,
                        hwnd,
                        inject_default=inject_default,
                        report=report,
                        shell_hwnd=shell,
                    )
        report["steps_run"] += 1
        return
    if op == "chord":
        mods = [_mod_vk(str(m)) for m in (step.get("modifiers") or [])]
        mods = [m for m in mods if m]
        for vk in mods:
            if inject == "sendinput":
                user32.SetForegroundWindow(shell)
                _send_key(vk, up=False)
            else:
                user32.PostMessageW(hwnd, WM_KEYDOWN, vk, 0)
        for child in step.get("steps") or []:
            if isinstance(child, dict):
                _run_step(
                    child,
                    hwnd,
                    inject_default=inject_default,
                    report=report,
                    shell_hwnd=shell,
                )
        for vk in reversed(mods):
            if inject == "sendinput":
                _send_key(vk, up=True)
            else:
                user32.PostMessageW(hwnd, WM_KEYUP, vk, 0)
        report["steps_run"] += 1
        return
    if op == "path":
        points = step.get("points") or []
        if inject == "sendinput":
            # PostMessage reaches view.pan even when SendInput hits occlusion;
            # SendInput still moves the OS cursor for visual review.
            _post_path(hwnd, points)
            _send_path(hwnd, points)
        else:
            _post_path(hwnd, points)
        report["steps_run"] += 1
        return
    if op == "pan_burst":
        count = max(1, int(step.get("count", 4)))
        x = int(step.get("x", 200))
        y = int(step.get("y", 300))
        dx = int(step.get("dx", 40))
        dy = int(step.get("dy", 24))
        between = max(0, int(step.get("pump_ms", 40))) / 1000.0
        if inject == "sendinput":
            # Activate once per burst — per-tick SetForegroundWindow blacks GDI.
            user32.SetForegroundWindow(shell)
        for i in range(count):
            x0 = x + (i % 5) * 8
            y0 = y + (i % 7) * 6
            x1 = x0 + dx
            y1 = y0 + dy
            if inject == "sendinput":
                _post_pan(hwnd, x0, y0, x1, y1)
                _send_pan(hwnd, x0, y0, x1, y1)
            else:
                _post_pan(hwnd, x0, y0, x1, y1)
            if between:
                time.sleep(between)
        report["steps_run"] += 1
        return
    if op == "wheel_burst":
        # Unidirectional: every tick uses the given delta (no i%2 sign flip).
        count = max(1, int(step.get("count", 3)))
        x = int(step.get("x", 400))
        y = int(step.get("y", 400))
        delta = int(step.get("delta", -120))
        between = max(0, int(step.get("pump_ms", 50))) / 1000.0
        if inject == "sendinput":
            # Activate once per burst — per-tick SetForegroundWindow blacks the GL view.
            user32.SetForegroundWindow(shell)
            sx, sy = _client_to_screen(hwnd, x, y)
            for _ in range(count):
                # mouseData is DWORD but wheel API treats it as signed.
                _send_mouse_abs(
                    sx,
                    sy,
                    MOUSEEVENTF_WHEEL,
                    data=ctypes.c_uint32(ctypes.c_int32(delta).value).value,
                )
                if between:
                    time.sleep(between)
        else:
            for _ in range(count):
                sx, sy = _client_to_screen(hwnd, x, y)
                wp = (delta << 16) & 0xFFFF0000
                lp = _lparam(sx & 0xFFFF, sy & 0xFFFF)
                # SendMessage so PreviewZoomScale runs before capture/settle.
                user32.SendMessageW(hwnd, WM_MOUSEWHEEL, wp, lp)
                if between:
                    time.sleep(between)
        user32.InvalidateRect(hwnd, None, False)
        user32.UpdateWindow(hwnd)
        report["steps_run"] += 1
        return

    if op == "pump":
        time.sleep(max(0, int(step.get("ms", 100))) / 1000.0)
    elif op == "window":
        action = step.get("action") or ""
        if action == "activate":
            # Activate the top-level shell — SetForegroundWindow on map_client
            # children often fails, leaving BitBlt/SendInput on the IDE.
            user32.ShowWindow(shell, 9)  # SW_RESTORE
            user32.ShowWindow(shell, 5)  # SW_SHOW
            user32.SetForegroundWindow(shell)
        elif action == "resize":
            w = int(step.get("w", 1280))
            h = int(step.get("h", 800))
            rc = wintypes.RECT()
            user32.GetWindowRect(shell, ctypes.byref(rc))
            user32.MoveWindow(shell, rc.left, rc.top, w, h, True)
        elif action == "move":
            x = int(step.get("x", 0))
            y = int(step.get("y", 0))
            rc = wintypes.RECT()
            user32.GetWindowRect(shell, ctypes.byref(rc))
            user32.MoveWindow(
                shell, x, y, rc.right - rc.left, rc.bottom - rc.top, True
            )
        elif action == "close":
            user32.PostMessageW(shell, 0x0010, 0, 0)  # WM_CLOSE
    elif op in ("click", "rclick", "dblclick"):
        x = int(step.get("x", 0))
        y = int(step.get("y", 0))
        right = op == "rclick" or step.get("button") == "right"
        if inject == "sendinput":
            _send_click(hwnd, x, y, right=right)
            if op == "dblclick":
                _send_click(hwnd, x, y, right=False)
        else:
            _post_click(hwnd, x, y, right=right)
            if op == "dblclick":
                lp = _lparam(x, y)
                user32.PostMessageW(hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON, lp)
                user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, lp)
    elif op == "drag":
        x0 = int(step.get("x0", 0))
        y0 = int(step.get("y0", 0))
        x1 = int(step.get("x1", x0))
        y1 = int(step.get("y1", y0))
        if inject == "sendinput":
            user32.SetForegroundWindow(shell)
            _post_pan(hwnd, x0, y0, x1, y1)
            _send_pan(hwnd, x0, y0, x1, y1)
        else:
            _post_pan(hwnd, x0, y0, x1, y1)
        report["steps_run"] += 1
        return
    elif op == "wheel":
        x = int(step.get("x", 0))
        y = int(step.get("y", 0))
        delta = int(step.get("delta", -120))
        if inject == "sendinput":
            sx, sy = _client_to_screen(hwnd, x, y)
            user32.SetForegroundWindow(shell)
            _send_mouse_abs(
                sx,
                sy,
                MOUSEEVENTF_WHEEL,
                data=ctypes.c_uint32(ctypes.c_int32(delta).value).value,
            )
        else:
            sx, sy = _client_to_screen(hwnd, x, y)
            wp = (delta << 16) & 0xFFFF0000
            lp = _lparam(sx & 0xFFFF, sy & 0xFFFF)
            user32.PostMessageW(hwnd, WM_MOUSEWHEEL, wp, lp)
    elif op == "key":
        vk = _vk(step)
        if not vk:
            raise ValueError(f"bad vk in {step}")
        if inject == "sendinput":
            user32.SetForegroundWindow(hwnd)
            _send_key(vk, up=False)
            _send_key(vk, up=True)
        else:
            user32.PostMessageW(hwnd, WM_KEYDOWN, vk, 0)
            user32.PostMessageW(hwnd, WM_KEYUP, vk, 0)
    elif op == "wm_command":
        # Post to top-level shell (menu / ribbon command IDs).
        cmd_id = int(step.get("id", 0) or 0)
        if cmd_id <= 0:
            raise ValueError(f"bad wm_command id in {step}")
        user32.PostMessageW(shell, WM_COMMAND, cmd_id & 0xFFFF, 0)
    elif op in (
        "select_map_tab",
        "catalog_tab",
        "inspector_tab",
        "mark",
        "run_command",
        "capture_bmp",
    ):
        report["steps_skipped"] += 1
        return
    else:
        report["steps_skipped"] += 1
        return
    report["steps_run"] += 1


