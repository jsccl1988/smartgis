# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""OS-level interact script runner: PostMessage and/or SendInput against HWND."""

from __future__ import annotations

import ctypes
import json
import time
from ctypes import wintypes
from pathlib import Path
from typing import Any

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_LBUTTONDBLCLK = 0x0203
WM_RBUTTONDOWN = 0x0204
WM_RBUTTONUP = 0x0205
WM_MOUSEWHEEL = 0x020A
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
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
MOUSEEVENTF_ABSOLUTE = 0x8000
KEYEVENTF_KEYUP = 0x0002

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
    sx = max(1, user32.GetSystemMetrics(0))
    sy = max(1, user32.GetSystemMetrics(1))
    return int(x * 65535 / sx), int(y * 65535 / sy)


def _send_mouse_abs(x: int, y: int, flags: int, data: int = 0) -> None:
    ax, ay = _abs_mouse(x, y)
    inp = INPUT()
    inp.type = INPUT_MOUSE
    inp.union.mi = MOUSEINPUT(
        ax, ay, data, flags | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE, 0, None
    )
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


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
        _send_mouse_abs(s[0], s[1], 0)
    s1 = _client_to_screen(hwnd, int(points[-1][0]), int(points[-1][1]))
    _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_LEFTUP)


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
        for i in range(count):
            x0 = x + (i % 5) * 8
            y0 = y + (i % 7) * 6
            x1 = x0 + dx
            y1 = y0 + dy
            if inject == "sendinput":
                s0 = _client_to_screen(hwnd, x0, y0)
                s1 = _client_to_screen(hwnd, x1, y1)
                user32.SetForegroundWindow(shell)
                # Explicit down → move → up so view.pan sees kMouseMove while
                # captured (down+up-only jumps skipped intermediate deltas).
                _send_mouse_abs(s0[0], s0[1], MOUSEEVENTF_LEFTDOWN)
                _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_MOVE)
                _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_LEFTUP)
            else:
                user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, _lparam(x0, y0))
                user32.PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, _lparam(x0, y0))
                user32.PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, _lparam(x1, y1))
                user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, _lparam(x1, y1))
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
            s0 = _client_to_screen(hwnd, x0, y0)
            s1 = _client_to_screen(hwnd, x1, y1)
            user32.SetForegroundWindow(shell)
            _send_mouse_abs(s0[0], s0[1], MOUSEEVENTF_LEFTDOWN)
            _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_MOVE)
            _send_mouse_abs(s1[0], s1[1], MOUSEEVENTF_LEFTUP)
        else:
            user32.PostMessageW(hwnd, WM_MOUSEMOVE, 0, _lparam(x0, y0))
            user32.PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, _lparam(x0, y0))
            user32.PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, _lparam(x1, y1))
            user32.PostMessageW(hwnd, WM_LBUTTONUP, 0, _lparam(x1, y1))
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


def _capture_zoom_sidecar(
    hwnd: int,
    path: Path,
) -> dict[str, Any]:
    """HWND BMP for zoom_before/after gate; writes .method.txt sidecar.

    Prefer the top-level shell HWND: BitBlt on some legacy map_client children
    can heap-corrupt the Python process. Use PrintWindow so an occluding IDE
    does not poison the zoom_gate pixel diff.
    """
    from ..record.hwnd import bring_hwnd_to_front, capture_hwnd_bmp_ex

    path = Path(path)
    bring_hwnd_to_front(int(hwnd))
    time.sleep(0.15)
    ok, frac = capture_hwnd_bmp_ex(int(hwnd), path, prefer_printwindow=True)
    return {
        "ok": bool(ok),
        "path": str(path),
        "near_black": round(float(frac), 4),
        "hwnd": int(hwnd),
    }


def run_script(
    script_path: Path,
    hwnd: int,
    *,
    inject_default: str = "postmessage",
    captures_root: Path | None = None,
    zoom_gate: dict[str, Any] | None = None,
    capture_hwnd: int | None = None,
) -> dict[str, Any]:
    report: dict[str, Any] = {
        "ok": True,
        "script": str(script_path),
        "hwnd": hwnd,
        "steps_run": 0,
        "steps_skipped": 0,
        "errors": [],
    }
    if not hwnd or not user32.IsWindow(hwnd):
        report["ok"] = False
        report["errors"].append("invalid_hwnd")
        return report

    zoom_cfg = zoom_gate if isinstance(zoom_gate, dict) else None
    zoom_done = False
    settle_ms = 90
    if zoom_cfg is not None:
        settle_ms = max(0, int(zoom_cfg.get("settle_ms", 90)))
    # Zoom sidecars: shell HWND when provided (safer than map_client BitBlt).
    zoom_hwnd = int(capture_hwnd) if capture_hwnd else int(hwnd)
    shell_hwnd = int(capture_hwnd) if capture_hwnd else int(hwnd)

    def _zoom_paths() -> tuple[Path, Path]:
        assert zoom_cfg is not None and captures_root is not None
        before_rel = str(zoom_cfg.get("before") or "legacy/_zoom_before.bmp")
        after_rel = str(zoom_cfg.get("after") or "legacy/_zoom_after.bmp")
        return (
            Path(captures_root) / before_rel.replace("\\", "/"),
            Path(captures_root) / after_rel.replace("\\", "/"),
        )

    def _capture_after_stable(path: Path) -> dict[str, Any]:
        """Recapture when the first after-frame is near-black (mid-redraw)."""
        after_cap = _capture_zoom_sidecar(zoom_hwnd, path)
        if float(after_cap.get("near_black") or 0.0) <= 0.85:
            return after_cap
        time.sleep(max(0.35, settle_ms / 1000.0))
        return _capture_zoom_sidecar(zoom_hwnd, path)

    def _run_zoom_burst(step: dict[str, Any]) -> dict[str, Any]:
        before_path, after_path = _zoom_paths()
        before_cap = _capture_zoom_sidecar(zoom_hwnd, before_path)
        _run_step(
            step,
            hwnd,
            inject_default=inject_default,
            report=report,
            shell_hwnd=shell_hwnd,
        )
        if settle_ms:
            time.sleep(settle_ms / 1000.0)
        after_cap = _capture_after_stable(after_path)
        return {
            "before": before_cap,
            "after": after_cap,
            "delta": int(step.get("delta", -120)),
            "count": int(step.get("count", 3)),
            "inject": str(step.get("inject") or inject_default),
        }

    def _maybe_zoom_wrap(step: dict[str, Any]) -> None:
        nonlocal zoom_done
        op = step.get("op")
        if (
            op == "wheel_burst"
            and zoom_cfg is not None
            and captures_root is not None
            and not zoom_done
        ):
            cap = _run_zoom_burst(step)
            before_path, after_path = _zoom_paths()
            try:
                same = before_path.read_bytes() == after_path.read_bytes()
            except OSError:
                same = True
            after_black = float((cap.get("after") or {}).get("near_black") or 0) > 0.85
            weak_zoom = False
            try:
                from ..score.bmp_io import pixel_diff_frac

                diff = float(
                    pixel_diff_frac(before_path, after_path, thresh=8) or 0.0
                )
                cap["pixel_diff_frac"] = round(diff, 6)
                # Shell chrome noise can make bytes differ while the map did not
                # zoom — escalate when below the suite zoom_gate floor.
                weak_zoom = diff < 0.001
            except Exception:  # noqa: BLE001
                weak_zoom = False
            if same or after_black or weak_zoom:
                # Escalate to SendInput once if PostMessage missed or frame was black.
                # Keep the original before BMP — re-running _run_zoom_burst would
                # overwrite before with a mid-zoom frame and collapse pixel_diff.
                time.sleep(max(0.4, settle_ms / 1000.0))
                step_si = dict(step)
                step_si["inject"] = "sendinput"
                try:
                    c0 = max(1, int(step_si.get("count", 3)))
                    step_si["count"] = max(2, c0 // 2)
                except (TypeError, ValueError):
                    step_si["count"] = 3
                before_keep = cap.get("before")
                _run_step(
                    step_si,
                    hwnd,
                    inject_default="sendinput",
                    report=report,
                    shell_hwnd=shell_hwnd,
                )
                if settle_ms:
                    time.sleep(settle_ms / 1000.0)
                after_cap = _capture_after_stable(after_path)
                cap = {
                    "before": before_keep,
                    "after": after_cap,
                    "delta": int(step_si.get("delta", -120)),
                    "count": int(step_si.get("count", 3)),
                    "inject": "sendinput",
                    "retried": True,
                }
                try:
                    from ..score.bmp_io import pixel_diff_frac

                    cap["pixel_diff_frac"] = round(
                        float(
                            pixel_diff_frac(before_path, after_path, thresh=8) or 0.0
                        ),
                        6,
                    )
                except Exception:  # noqa: BLE001
                    pass
            report["zoom_capture"] = cap
            zoom_done = True
            return
        _run_step(
            step,
            hwnd,
            inject_default=inject_default,
            report=report,
            shell_hwnd=shell_hwnd,
        )

    # Prefer Interact DSL (.il); legacy JSON remains as fallback.
    if str(script_path).lower().endswith(".il"):
        from .dsl import exec_os, parse_interact_file

        try:
            _name, stmts = parse_interact_file(script_path)
        except Exception as exc:  # noqa: BLE001
            report["ok"] = False
            report["errors"].append(f"dsl_parse: {exc}")
            return report
        actions: list[dict[str, Any]] = []
        exec_os(stmts, emit=actions.append, inject_default=inject_default)
        chord_stack: list[list[int]] = []
        for step in actions:
            try:
                op = step.get("op")
                if op == "chord_begin":
                    mods = []
                    for name in step.get("modifiers") or []:
                        vk = _mod_vk(str(name))
                        if vk:
                            mods.append(vk)
                            if str(step.get("inject") or inject_default).lower() == "sendinput":
                                user32.SetForegroundWindow(shell_hwnd)
                                _send_key(vk, up=False)
                            else:
                                user32.PostMessageW(hwnd, WM_KEYDOWN, vk, 0)
                    chord_stack.append(mods)
                    report["steps_run"] += 1
                    continue
                if op == "chord_end":
                    mods = chord_stack.pop() if chord_stack else []
                    for vk in reversed(mods):
                        if str(step.get("inject") or inject_default).lower() == "sendinput":
                            _send_key(vk, up=True)
                        else:
                            user32.PostMessageW(hwnd, WM_KEYUP, vk, 0)
                    report["steps_run"] += 1
                    continue
                _maybe_zoom_wrap(step)
            except Exception as exc:  # noqa: BLE001
                report["ok"] = False
                report["errors"].append(f"{step.get('op')}: {exc}")
                break
        return report

    raw = json.loads(script_path.read_text(encoding="utf-8"))
    steps = raw.get("steps") or []
    for step in steps:
        if not isinstance(step, dict):
            continue
        try:
            _maybe_zoom_wrap(step)
        except Exception as exc:  # noqa: BLE001
            report["ok"] = False
            report["errors"].append(f"{step.get('op')}: {exc}")
            break
    return report
