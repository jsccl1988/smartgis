# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Win32 SendInput / PostMessage primitives for OS interact inject."""

from __future__ import annotations

import ctypes
import json
import time
from ctypes import wintypes
from pathlib import Path
from typing import Any

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

# Without HWND argtypes, 64-bit IsWindow can reject valid shell HWNDs.
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowVisible.restype = wintypes.BOOL

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_LBUTTONDBLCLK = 0x0203
WM_RBUTTONDOWN = 0x0204
WM_RBUTTONUP = 0x0205
WM_MOUSEWHEEL = 0x020A
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_COMMAND = 0x0111
MK_LBUTTON = 0x0001
MK_RBUTTON = 0x0002

INPUT_MOUSE = 0
INPUT_KEYBOARD = 1
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_VIRTUALDESK = 0x4000
MOUSEEVENTF_ABSOLUTE = 0x8000
KEYEVENTF_KEYUP = 0x0002

# Align SendInput absolute coords with GetWindowRect / ClientToScreen (physical).
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(2)  # PROCESS_PER_MONITOR_DPI_AWARE
except Exception:  # noqa: BLE001
    try:
        user32.SetProcessDPIAware()
    except Exception:  # noqa: BLE001
        pass

VK_MAP = {
    "ESCAPE": 0x1B,
    "Esc": 0x1B,
    "RETURN": 0x0D,
    "ENTER": 0x0D,
    "TAB": 0x09,
    "SPACE": 0x20,
}


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", wintypes.LONG),
        ("dy", wintypes.LONG),
        ("mouseData", wintypes.DWORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
    ]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [
        ("wVk", wintypes.WORD),
        ("wScan", wintypes.WORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
    ]


class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [
        ("uMsg", wintypes.DWORD),
        ("wParamL", wintypes.WORD),
        ("wParamH", wintypes.WORD),
    ]


class INPUT_UNION(ctypes.Union):
    _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT), ("hi", HARDWAREINPUT)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wintypes.DWORD), ("union", INPUT_UNION)]


user32.PostMessageW.argtypes = [
    wintypes.HWND,
    wintypes.UINT,
    wintypes.WPARAM,
    wintypes.LPARAM,
]
user32.PostMessageW.restype = wintypes.BOOL
user32.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
user32.ClientToScreen.restype = wintypes.BOOL
user32.SetForegroundWindow.argtypes = [wintypes.HWND]
user32.SetForegroundWindow.restype = wintypes.BOOL
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.ShowWindow.restype = wintypes.BOOL
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetWindowRect.restype = wintypes.BOOL
user32.MoveWindow.argtypes = [
    wintypes.HWND,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.BOOL,
]
user32.MoveWindow.restype = wintypes.BOOL
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL
user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
user32.FindWindowW.restype = wintypes.HWND
user32.GetSystemMetrics.argtypes = [ctypes.c_int]
user32.GetSystemMetrics.restype = ctypes.c_int
user32.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
user32.SendInput.restype = wintypes.UINT
user32.SendMessageW.argtypes = [
    wintypes.HWND,
    wintypes.UINT,
    wintypes.WPARAM,
    wintypes.LPARAM,
]
user32.SendMessageW.restype = wintypes.LPARAM
user32.InvalidateRect.argtypes = [
    wintypes.HWND,
    ctypes.c_void_p,
    wintypes.BOOL,
]
user32.InvalidateRect.restype = wintypes.BOOL
user32.UpdateWindow.argtypes = [wintypes.HWND]
user32.UpdateWindow.restype = wintypes.BOOL


def _lparam(x: int, y: int) -> int:
    # Pack as unsigned 16-bit halves (signed screen coords on multi-mon).
    return ((y & 0xFFFF) << 16) | (x & 0xFFFF)


def find_window_by_title(
    title: str,
    timeout_sec: float = 30.0,
    *,
    pid: int | None = None,
) -> int:
    """Exact-title FindWindowW poll; when |pid| is set, ignore foreign owners."""
    from ..record.hwnd import hwnd_pid

    want_pid = int(pid) if pid is not None else None
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        hwnd = user32.FindWindowW(None, title)
        if hwnd and user32.IsWindow(hwnd):
            if want_pid is None or hwnd_pid(int(hwnd)) == want_pid:
                return int(hwnd)
        time.sleep(0.2)
    return 0


def _step_allows_os(step: dict[str, Any]) -> bool:
    drivers = step.get("drivers")
    if not drivers:
        return True
    return "os" in drivers


def _vk(step: dict[str, Any]) -> int:
    raw = step.get("vk")
    if isinstance(raw, int):
        return raw
    if isinstance(raw, str):
        if raw in VK_MAP:
            return VK_MAP[raw]
        if len(raw) == 1:
            c = raw.upper()
            if "A" <= c <= "Z" or "0" <= c <= "9":
                return ord(c)
    return 0


def _client_to_screen(hwnd: int, x: int, y: int) -> tuple[int, int]:
    pt = wintypes.POINT(x, y)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    return int(pt.x), int(pt.y)


def _abs_mouse(x: int, y: int) -> tuple[int, int]:
    """Normalize screen px → SendInput absolute (0..65535) on the virtual desk."""
    ox = int(user32.GetSystemMetrics(76))  # SM_XVIRTUALSCREEN
    oy = int(user32.GetSystemMetrics(77))  # SM_YVIRTUALSCREEN
    sx = max(1, int(user32.GetSystemMetrics(78)))  # SM_CXVIRTUALSCREEN
    sy = max(1, int(user32.GetSystemMetrics(79)))  # SM_CYVIRTUALSCREEN
    return int((x - ox) * 65535 / sx), int((y - oy) * 65535 / sy)


def _send_mouse_abs(x: int, y: int, flags: int, data: int = 0) -> None:
    ax, ay = _abs_mouse(x, y)
    inp = INPUT()
    inp.type = INPUT_MOUSE
    inp.union.mi = MOUSEINPUT(
        ax,
        ay,
        data,
        flags | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | MOUSEEVENTF_MOVE,
        0,
        None,
    )
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def _post_pan(hwnd: int, x0: int, y0: int, x1: int, y1: int) -> None:
    """Client-coord pan via PostMessage (DPI-safe; reaches view.pan SetCapture)."""
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, _lparam(x0, y0))
    user32.PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, _lparam(x0, y0))
    mid_x = (x0 + x1) // 2
    mid_y = (y0 + y1) // 2
    if mid_x != x0 or mid_y != y0:
        user32.PostMessageW(
            hwnd, WM_MOUSEMOVE, MK_LBUTTON, _lparam(mid_x, mid_y)
        )
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, _lparam(x1, y1))
    user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, _lparam(x1, y1))


def _send_pan(hwnd: int, x0: int, y0: int, x1: int, y1: int) -> None:
    """Screen-coord pan via SendInput (cursor follows; needs virtual-desk abs)."""
    s0 = _client_to_screen(hwnd, x0, y0)
    s1 = _client_to_screen(hwnd, x1, y1)
    _send_mouse_abs(s0[0], s0[1], MOUSEEVENTF_LEFTDOWN)
    time.sleep(0.01)
    mid_x = (s0[0] + s1[0]) // 2
    mid_y = (s0[1] + s1[1]) // 2
    # MOVE only — re-sending LEFTDOWN resets view.pan capture origin.
    _send_mouse_abs(mid_x, mid_y, MOUSEEVENTF_MOVE)
    time.sleep(0.01)
    _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_MOVE)
    time.sleep(0.01)
    _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_LEFTUP)


def _send_key(vk: int, up: bool = False) -> None:
    inp = INPUT()
    inp.type = INPUT_KEYBOARD
    inp.union.ki = KEYBDINPUT(vk, 0, KEYEVENTF_KEYUP if up else 0, 0, None)
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def _post_click(hwnd: int, x: int, y: int, right: bool = False) -> None:
    down = WM_RBUTTONDOWN if right else WM_LBUTTONDOWN
    up = WM_RBUTTONUP if right else WM_LBUTTONUP
    mk = MK_RBUTTON if right else MK_LBUTTON
    lp = _lparam(x, y)
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, lp)
    user32.PostMessageW(hwnd, down, mk, lp)
    user32.PostMessageW(hwnd, up, 0, lp)


def _send_click(hwnd: int, x: int, y: int, right: bool = False) -> None:
    sx, sy = _client_to_screen(hwnd, x, y)
    user32.SetForegroundWindow(hwnd)
    down = MOUSEEVENTF_RIGHTDOWN if right else MOUSEEVENTF_LEFTDOWN
    up = MOUSEEVENTF_RIGHTUP if right else MOUSEEVENTF_LEFTUP
    _send_mouse_abs(sx, sy, down)
    _send_mouse_abs(sx, sy, up)


def _mod_vk(name: str) -> int:
    key = name.upper()
    if key in ("CTRL", "CONTROL"):
        return 0x11  # VK_CONTROL
    if key == "SHIFT":
        return 0x10
    if key in ("ALT", "MENU"):
        return 0x12
    return VK_MAP.get(name, 0) or VK_MAP.get(key, 0)


def _post_path(hwnd: int, points: list) -> None:
    if len(points) < 2:
        raise ValueError("path needs >=2 points")
    x0, y0 = int(points[0][0]), int(points[0][1])
    user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, _lparam(x0, y0))
    user32.PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, _lparam(x0, y0))
    for pt in points[1:]:
        x, y = int(pt[0]), int(pt[1])
        user32.PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, _lparam(x, y))
    x1, y1 = int(points[-1][0]), int(points[-1][1])
    user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, _lparam(x1, y1))


def _send_path(hwnd: int, points: list) -> None:
    if len(points) < 2:
        raise ValueError("path needs >=2 points")
    user32.SetForegroundWindow(hwnd)
    s0 = _client_to_screen(hwnd, int(points[0][0]), int(points[0][1]))
    _send_mouse_abs(s0[0], s0[1], MOUSEEVENTF_LEFTDOWN)
    for pt in points[1:-1]:
        s = _client_to_screen(hwnd, int(pt[0]), int(pt[1]))
        _send_mouse_abs(s[0], s[1], MOUSEEVENTF_MOVE)
        time.sleep(0.005)
    s1 = _client_to_screen(hwnd, int(points[-1][0]), int(points[-1][1]))
    _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_MOVE)
    time.sleep(0.01)
    _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_LEFTUP)


