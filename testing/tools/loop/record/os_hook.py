# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Low-level mouse/keyboard hooks scoped to a target HWND (client coords)."""

from __future__ import annotations

import ctypes
import threading
import time
from ctypes import wintypes
from typing import Any, Callable

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

WH_KEYBOARD_LL = 13
WH_MOUSE_LL = 14
WM_QUIT = 0x0012
WM_HOTKEY = 0x0312
HC_ACTION = 0

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_RBUTTONDOWN = 0x0204
WM_RBUTTONUP = 0x0205
WM_MOUSEWHEEL = 0x020A
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_SYSKEYDOWN = 0x0104
WM_SYSKEYUP = 0x0105

GA_ROOT = 2
MOD_CONTROL = 0x0002
MOD_SHIFT = 0x0004
VK_F9 = 0x78

LLKHF_INJECTED = 0x10
LLMHF_INJECTED = 0x01

VK_NAME = {
    0x1B: "ESCAPE",
    0x0D: "RETURN",
    0x09: "TAB",
    0x20: "SPACE",
    0x08: "BACK",
    0x2E: "DELETE",
    0x25: "LEFT",
    0x26: "UP",
    0x27: "RIGHT",
    0x28: "DOWN",
    0x10: "SHIFT",
    0x11: "CTRL",
    0x12: "ALT",
}


class POINT(ctypes.Structure):
    _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]


class MSLLHOOKSTRUCT(ctypes.Structure):
    _fields_ = [
        ("pt", POINT),
        ("mouseData", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
    ]


class KBDLLHOOKSTRUCT(ctypes.Structure):
    _fields_ = [
        ("vkCode", wintypes.DWORD),
        ("scanCode", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong)),
    ]


LowLevelMouseProc = ctypes.WINFUNCTYPE(
    ctypes.c_long, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM
)
LowLevelKeyboardProc = ctypes.WINFUNCTYPE(
    ctypes.c_long, ctypes.c_int, wintypes.WPARAM, wintypes.LPARAM
)

user32.SetWindowsHookExW.argtypes = [
    ctypes.c_int,
    ctypes.c_void_p,
    wintypes.HINSTANCE,
    wintypes.DWORD,
]
user32.SetWindowsHookExW.restype = wintypes.HHOOK
user32.CallNextHookEx.argtypes = [
    wintypes.HHOOK,
    ctypes.c_int,
    wintypes.WPARAM,
    wintypes.LPARAM,
]
user32.CallNextHookEx.restype = ctypes.c_long
user32.UnhookWindowsHookEx.argtypes = [wintypes.HHOOK]
user32.UnhookWindowsHookEx.restype = wintypes.BOOL
user32.GetMessageW.argtypes = [
    ctypes.POINTER(wintypes.MSG),
    wintypes.HWND,
    wintypes.UINT,
    wintypes.UINT,
]
user32.GetMessageW.restype = ctypes.c_int
user32.TranslateMessage.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.DispatchMessageW.argtypes = [ctypes.POINTER(wintypes.MSG)]
user32.PostThreadMessageW.argtypes = [
    wintypes.DWORD,
    wintypes.UINT,
    wintypes.WPARAM,
    wintypes.LPARAM,
]
user32.PostThreadMessageW.restype = wintypes.BOOL
user32.WindowFromPoint.argtypes = [POINT]
user32.WindowFromPoint.restype = wintypes.HWND
user32.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
user32.GetAncestor.restype = wintypes.HWND
user32.ScreenToClient.argtypes = [wintypes.HWND, ctypes.POINTER(POINT)]
user32.ScreenToClient.restype = wintypes.BOOL
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL
user32.RegisterHotKey.argtypes = [
    wintypes.HWND,
    ctypes.c_int,
    wintypes.UINT,
    wintypes.UINT,
]
user32.RegisterHotKey.restype = wintypes.BOOL
user32.UnregisterHotKey.argtypes = [wintypes.HWND, ctypes.c_int]
user32.UnregisterHotKey.restype = wintypes.BOOL
kernel32.GetCurrentThreadId.restype = wintypes.DWORD

EventSink = Callable[[dict[str, Any]], None]


def _vk_name(vk: int) -> str:
    if vk in VK_NAME:
        return VK_NAME[vk]
    if 0x30 <= vk <= 0x39 or 0x41 <= vk <= 0x5A:
        return chr(vk)
    if 0x70 <= vk <= 0x7B:
        return f"F{vk - 0x6F}"
    return f"VK_{vk}"


user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetClientRect.restype = wintypes.BOOL
user32.GetForegroundWindow.restype = wintypes.HWND


def _under_target(screen_x: int, screen_y: int, target: int) -> bool:
    if not target or not user32.IsWindow(target):
        return False
    # Prefer hit-test under cursor; also accept client-rect of target (SendInput /
    # multi-monitor / child HWND races).
    pt = POINT(screen_x, screen_y)
    hwnd = int(user32.WindowFromPoint(pt) or 0)
    if hwnd:
        root = int(user32.GetAncestor(hwnd, GA_ROOT) or hwnd)
        if root == int(target):
            return True
    client = POINT(screen_x, screen_y)
    if user32.ScreenToClient(target, ctypes.byref(client)):
        rc = wintypes.RECT()
        if user32.GetClientRect(target, ctypes.byref(rc)):
            if (
                rc.left <= client.x < rc.right
                and rc.top <= client.y < rc.bottom
            ):
                return True
    return int(user32.GetForegroundWindow() or 0) == int(target)


def _to_client(target: int, screen_x: int, screen_y: int) -> tuple[int, int]:
    pt = POINT(screen_x, screen_y)
    if target and user32.IsWindow(target):
        user32.ScreenToClient(target, ctypes.byref(pt))
    return int(pt.x), int(pt.y)


class OsInputHook:
    """Install LL hooks; emit raw OS events until stop()."""

    def __init__(
        self,
        *,
        target_hwnd: int,
        on_event: EventSink,
        t0: float | None = None,
        hotkey_id: int = 1,
        enable_hotkey: bool = True,
        ignore_injected: bool = True,
    ) -> None:
        self.target_hwnd = int(target_hwnd)
        self.on_event = on_event
        self.t0 = time.perf_counter() if t0 is None else t0
        self.hotkey_id = hotkey_id
        self.enable_hotkey = enable_hotkey
        self._stop = threading.Event()
        self._hotkey_hit = threading.Event()
        self._thread: threading.Thread | None = None
        self._tid = 0
        self._mouse_hook = None
        self._key_hook = None
        self._mouse_proc = None
        self._key_proc = None
        self._ignore_injected = ignore_injected
        self._lbutton_down = False
        self._suppress_hotkey_keys = False

    def _t_ms(self) -> int:
        return int((time.perf_counter() - self.t0) * 1000)

    def _emit(self, payload: dict[str, Any]) -> None:
        payload.setdefault("src", "os")
        payload.setdefault("t_ms", self._t_ms())
        self.on_event(payload)

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop.clear()
        self._hotkey_hit.clear()
        self._thread = threading.Thread(target=self._run, name="os-input-hook", daemon=True)
        self._thread.start()
        # Wait briefly for hooks to install.
        for _ in range(50):
            if self._tid:
                break
            time.sleep(0.02)

    def stop(self) -> None:
        self._stop.set()
        tid = self._tid
        if tid:
            user32.PostThreadMessageW(tid, WM_QUIT, 0, 0)
        if self._thread:
            self._thread.join(timeout=5.0)
            self._thread = None

    def hotkey_triggered(self) -> bool:
        return self._hotkey_hit.is_set()

    def wait_stop(self, timeout: float | None = None) -> bool:
        """Block until stop() or hotkey. Returns True if hotkey stopped."""
        deadline = None if timeout is None else time.time() + timeout
        while not self._stop.is_set():
            if self._hotkey_hit.wait(0.1):
                self.stop()
                return True
            if deadline is not None and time.time() >= deadline:
                return False
        return self._hotkey_hit.is_set()

    def _run(self) -> None:
        self._tid = int(kernel32.GetCurrentThreadId())

        def mouse_cb(n_code: int, w_param: int, l_param: int) -> int:
            if n_code == HC_ACTION and not self._stop.is_set():
                info = ctypes.cast(l_param, ctypes.POINTER(MSLLHOOKSTRUCT)).contents
                if self._ignore_injected and (info.flags & LLMHF_INJECTED):
                    return int(user32.CallNextHookEx(self._mouse_hook, n_code, w_param, l_param))
                sx, sy = int(info.pt.x), int(info.pt.y)
                if _under_target(sx, sy, self.target_hwnd):
                    cx, cy = _to_client(self.target_hwnd, sx, sy)
                    msg = int(w_param)
                    if msg == WM_LBUTTONDOWN:
                        self._lbutton_down = True
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "down",
                                "button": "left",
                                "x": cx,
                                "y": cy,
                            }
                        )
                    elif msg == WM_LBUTTONUP:
                        self._lbutton_down = False
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "up",
                                "button": "left",
                                "x": cx,
                                "y": cy,
                            }
                        )
                    elif msg == WM_RBUTTONDOWN:
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "down",
                                "button": "right",
                                "x": cx,
                                "y": cy,
                            }
                        )
                    elif msg == WM_RBUTTONUP:
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "up",
                                "button": "right",
                                "x": cx,
                                "y": cy,
                            }
                        )
                    elif msg == WM_MOUSEMOVE and self._lbutton_down:
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "move",
                                "button": "left",
                                "x": cx,
                                "y": cy,
                            }
                        )
                    elif msg == WM_MOUSEWHEEL:
                        # high word is signed delta
                        raw = ctypes.c_short((info.mouseData >> 16) & 0xFFFF).value
                        self._emit(
                            {
                                "kind": "mouse",
                                "action": "wheel",
                                "x": cx,
                                "y": cy,
                                "delta": int(raw),
                            }
                        )
            return int(user32.CallNextHookEx(self._mouse_hook, n_code, w_param, l_param))

        def key_cb(n_code: int, w_param: int, l_param: int) -> int:
            if n_code == HC_ACTION and not self._stop.is_set():
                info = ctypes.cast(l_param, ctypes.POINTER(KBDLLHOOKSTRUCT)).contents
                if self._ignore_injected and (info.flags & LLKHF_INJECTED):
                    return int(user32.CallNextHookEx(self._key_hook, n_code, w_param, l_param))
                vk = int(info.vkCode)
                msg = int(w_param)
                # Only record keys while target (or its root) is foreground.
                fg = int(user32.GetForegroundWindow() or 0)
                fg_root = int(user32.GetAncestor(fg, GA_ROOT) or fg) if fg else 0
                if fg_root != int(self.target_hwnd) and fg != int(self.target_hwnd):
                    return int(user32.CallNextHookEx(self._key_hook, n_code, w_param, l_param))
                # Drop Ctrl/Shift/F9 while hotkey chord is being pressed.
                if vk in (0x10, 0x11, VK_F9) and self.enable_hotkey:
                    return int(user32.CallNextHookEx(self._key_hook, n_code, w_param, l_param))
                if msg in (WM_KEYDOWN, WM_SYSKEYDOWN):
                    self._emit(
                        {
                            "kind": "key",
                            "action": "down",
                            "vk": vk,
                            "name": _vk_name(vk),
                        }
                    )
                elif msg in (WM_KEYUP, WM_SYSKEYUP):
                    self._emit(
                        {
                            "kind": "key",
                            "action": "up",
                            "vk": vk,
                            "name": _vk_name(vk),
                        }
                    )
            return int(user32.CallNextHookEx(self._key_hook, n_code, w_param, l_param))

        self._mouse_proc = LowLevelMouseProc(mouse_cb)
        self._key_proc = LowLevelKeyboardProc(key_cb)
        self._mouse_hook = user32.SetWindowsHookExW(
            WH_MOUSE_LL, self._mouse_proc, None, 0
        )
        self._key_hook = user32.SetWindowsHookExW(
            WH_KEYBOARD_LL, self._key_proc, None, 0
        )
        hotkey_ok = False
        if self.enable_hotkey:
            hotkey_ok = bool(
                user32.RegisterHotKey(None, self.hotkey_id, MOD_CONTROL | MOD_SHIFT, VK_F9)
            )

        msg = wintypes.MSG()
        while not self._stop.is_set():
            r = user32.GetMessageW(ctypes.byref(msg), None, 0, 0)
            if r == 0 or r == -1:
                break
            if msg.message == WM_HOTKEY and int(msg.wParam) == self.hotkey_id:
                self._hotkey_hit.set()
                break
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))

        if hotkey_ok:
            user32.UnregisterHotKey(None, self.hotkey_id)
        if self._mouse_hook:
            user32.UnhookWindowsHookEx(self._mouse_hook)
            self._mouse_hook = None
        if self._key_hook:
            user32.UnhookWindowsHookEx(self._key_hook)
            self._key_hook = None
        self._tid = 0
