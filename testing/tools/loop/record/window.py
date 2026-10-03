# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""HWND discovery, geometry, and z-order helpers for harness recording."""

from __future__ import annotations

import ctypes
import time
from ctypes import wintypes
from typing import Any, Callable

from .win32 import (
    SM_CXSCREEN,
    SM_CXVIRTUALSCREEN,
    SM_CYSCREEN,
    SM_CYVIRTUALSCREEN,
    SM_XVIRTUALSCREEN,
    SM_YVIRTUALSCREEN,
    kernel32,
    user32,
)


def window_rect(hwnd: int) -> tuple[int, int, int, int]:
    rc = wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(rc)):
        return 0, 0, 0, 0
    return int(rc.left), int(rc.top), int(rc.right), int(rc.bottom)


# Compat alias used by plain_browse_capture / older call sites.
_window_rect = window_rect


def window_area(hwnd: int) -> int:
    left, top, right, bottom = window_rect(hwnd)
    return max(0, right - left) * max(0, bottom - top)


_window_area = window_area


def hwnd_pid(hwnd: int) -> int:
    """Owning process id for |hwnd|, or 0 when unavailable."""
    if not hwnd or not user32.IsWindow(hwnd):
        return 0
    pid = wintypes.DWORD(0)
    user32.GetWindowThreadProcessId(int(hwnd), ctypes.byref(pid))
    return int(pid.value)


def is_window(hwnd: int) -> bool:
    return bool(hwnd) and bool(user32.IsWindow(int(hwnd)))


def class_name(hwnd: int) -> str:
    buf = ctypes.create_unicode_buffer(256)
    n = user32.GetClassNameW(hwnd, buf, 256)
    if n <= 0:
        return ""
    return buf.value or ""


_class_name = class_name


def virtual_screen() -> dict[str, int]:
    return {
        "origin_x": int(user32.GetSystemMetrics(SM_XVIRTUALSCREEN)),
        "origin_y": int(user32.GetSystemMetrics(SM_YVIRTUALSCREEN)),
        "width": int(user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)),
        "height": int(user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)),
        "primary_w": int(user32.GetSystemMetrics(SM_CXSCREEN)),
        "primary_h": int(user32.GetSystemMetrics(SM_CYSCREEN)),
    }


_virtual_screen = virtual_screen


def rect_fully_on_primary(left: int, top: int, right: int, bottom: int) -> bool:
    """True when the HWND lies on the primary monitor (0,0)-(pw,ph).

    Maximized top-level frames often report left/top ≈ -8 (Aero borders). Treat
    a small slop as still on-primary so capture prefers BitBlt; PrintWindow-first
    on GL/D3D shells frequently yields a solid-black buffer (legacy.browse.3d).
    """
    pw = int(user32.GetSystemMetrics(SM_CXSCREEN))
    ph = int(user32.GetSystemMetrics(SM_CYSCREEN))
    slop = 16
    return (
        left >= -slop
        and top >= -slop
        and right <= pw + slop
        and bottom <= ph + slop
        and right > left
        and bottom > top
    )


_rect_fully_on_primary = rect_fully_on_primary


def bring_hwnd_to_front(
    hwnd: int,
    *,
    stay_topmost: bool = False,
    restore: bool = True,
) -> bool:
    """Restore + foreground so BitBlt is not occluded by IDE chrome.

    Windows often denies SetForegroundWindow from a background Python host.
    Try TOPMOST pulse + AttachThreadInput so PrintWindow/BitBlt see the shell.
    When |stay_topmost| is True (FlyCube DXGI BitBlt), leave HWND_TOPMOST so
    IDE does not cover the present rect mid-capture.

    |restore|: SW_RESTORE can re-layout FlyCube Present (TOOLWINDOW popup) and
    pull shell chrome into the BitBlt rect. Prefer restore=False for present.
    """
    if not hwnd or not user32.IsWindow(hwnd):
        return False
    hwnd_i = int(hwnd)
    if restore:
        user32.ShowWindow(hwnd_i, 9)  # SW_RESTORE
        user32.ShowWindow(hwnd_i, 5)  # SW_SHOW
    else:
        user32.ShowWindow(hwnd_i, 4)  # SW_SHOWNOACTIVATE
    # HWND_TOPMOST then optionally HWND_NOTOPMOST.
    HWND_TOPMOST = -1
    HWND_NOTOPMOST = -2
    SWP_NOMOVE = 0x0002
    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040
    SWP_NOACTIVATE = 0x0010
    flags = SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW
    if not restore:
        flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
    user32.SetWindowPos(hwnd_i, HWND_TOPMOST, 0, 0, 0, 0, flags)
    if not stay_topmost:
        user32.SetWindowPos(hwnd_i, HWND_NOTOPMOST, 0, 0, 0, 0, flags)
    if not restore:
        # Soft path: z-order only; do not steal focus from the shell chrome.
        return True
    fg = int(user32.GetForegroundWindow() or 0)
    ok = bool(user32.SetForegroundWindow(hwnd_i))
    if (not ok or int(user32.GetForegroundWindow() or 0) != hwnd_i) and fg and fg != hwnd_i:
        # Attach to the current foreground thread so focus steal is allowed.
        my_tid = int(kernel32.GetCurrentThreadId())
        fg_pid = wintypes.DWORD(0)
        fg_tid = int(user32.GetWindowThreadProcessId(fg, ctypes.byref(fg_pid)))
        attached = False
        if fg_tid and fg_tid != my_tid:
            attached = bool(user32.AttachThreadInput(my_tid, fg_tid, True))
        try:
            user32.BringWindowToTop(hwnd_i)
            ok = bool(user32.SetForegroundWindow(hwnd_i))
        finally:
            if attached:
                user32.AttachThreadInput(my_tid, fg_tid, False)
    return ok or int(user32.GetForegroundWindow() or 0) == hwnd_i


def drop_topmost(hwnd: int) -> None:
    """Clear HWND_TOPMOST after a capture pulse."""
    if not hwnd or not user32.IsWindow(hwnd):
        return
    HWND_NOTOPMOST = -2
    user32.SetWindowPos(
        int(hwnd),
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        0x0002 | 0x0001 | 0x0010 | 0x0040,  # NOMOVE|NOSIZE|NOACTIVATE|SHOWWINDOW
    )


def _window_title(hwnd: int) -> str:
    n = user32.GetWindowTextLengthW(hwnd)
    if n <= 0:
        return ""
    buf = ctypes.create_unicode_buffer(n + 1)
    user32.GetWindowTextW(hwnd, buf, n + 1)
    return buf.value or ""


def _collect_top_level(
    accept: Callable[[int, str], bool],
) -> list[tuple[int, str]]:
    """Enumerate visible top-level windows; keep those where |accept| is True."""
    found: list[tuple[int, str]] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def _enum(hwnd: int, _lp: int) -> bool:  # type: ignore[misc]
        if not user32.IsWindowVisible(hwnd):
            return True
        title = _window_title(hwnd)
        if accept(int(hwnd), title):
            found.append((int(hwnd), title))
        return True

    user32.EnumWindows(_enum, 0)
    return found


def _largest(found: list[tuple[int, str]]) -> tuple[int, str]:
    found.sort(key=lambda item: window_area(item[0]), reverse=True)
    return found[0]


def find_window_by_title_substr(
    substr: str,
    timeout_sec: float = 30.0,
    *,
    pid: int | None = None,
) -> tuple[int, str]:
    """Return (hwnd, title) for the largest visible top-level window matching substr.

    Dual-monitor: EnumWindows may see several matches (stale/minimized chrome).
    Prefer the largest on-screen rect so recording does not bind a tiny leftover.

    When |pid| is set, only windows owned by that process match — avoids binding
    Cursor/VS Code titles that contain the repo name (e.g. ``… - smartgis - Cursor``).
    """
    needle = (substr or "").strip()
    if not needle:
        return 0, ""
    want_pid = int(pid) if pid is not None else None
    deadline = time.time() + timeout_sec
    needle_l = needle.lower()

    def _accept(hwnd: int, title: str) -> bool:
        if want_pid is not None and hwnd_pid(hwnd) != want_pid:
            return False
        return bool(title) and needle_l in title.lower()

    while time.time() < deadline:
        found = _collect_top_level(_accept)
        if found:
            return _largest(found)
        # Exact title fast-path (FindWindowW) — still honor |pid| when set.
        hwnd = int(user32.FindWindowW(None, needle) or 0)
        if hwnd and user32.IsWindow(hwnd):
            if want_pid is None or hwnd_pid(hwnd) == want_pid:
                return hwnd, needle
        time.sleep(0.2)
    return 0, ""


def find_top_level_hwnd_for_pid(
    pid: int,
    timeout_sec: float = 30.0,
    *,
    min_area: int = 0,
    title_substr: str = "",
) -> tuple[int, str]:
    """Largest visible top-level window owned by |pid|.

    |min_area| skips splash / toast HWNDs. |title_substr| optionally requires a
    case-insensitive title match (empty = any title).
    """
    want = int(pid)
    if want <= 0:
        return 0, ""
    needle = (title_substr or "").strip().lower()
    deadline = time.time() + timeout_sec
    min_a = int(min_area)

    def _accept(hwnd: int, title: str) -> bool:
        if hwnd_pid(hwnd) != want:
            return False
        if window_area(hwnd) < min_a:
            return False
        if needle and needle not in title.lower():
            return False
        return True

    while time.time() < deadline:
        found = _collect_top_level(_accept)
        if found:
            return _largest(found)
        time.sleep(0.2)
    return 0, ""


def wait_stable_shell_hwnd(
    pid: int,
    *,
    title_substr: str = "",
    timeout_sec: float = 45.0,
    min_area: int = 200_000,
    stable_ms: int = 600,
) -> tuple[int, str]:
    """Find a large top-level HWND for |pid| that still exists after |stable_ms|.

    Prefer titles containing ``EDIT`` (MDI main frame) over a bare product title
    splash that closes shortly after create.
    """
    deadline = time.time() + max(1.0, float(timeout_sec))
    settle = max(0.05, float(stable_ms) / 1000.0)
    last_title = ""
    needle = (title_substr or "").strip().lower()

    def _pick() -> tuple[int, str]:
        # Prefer MDI Edit frame when present.
        hwnd, title = find_top_level_hwnd_for_pid(
            pid,
            timeout_sec=0.35,
            min_area=min_area,
            title_substr="EDIT",
        )
        if hwnd:
            return hwnd, title
        hwnd, title = find_top_level_hwnd_for_pid(
            pid,
            timeout_sec=0.35,
            min_area=min_area,
            title_substr=title_substr,
        )
        if hwnd:
            return hwnd, title
        return find_top_level_hwnd_for_pid(
            pid, timeout_sec=0.35, min_area=min_area, title_substr=""
        )

    while time.time() < deadline:
        hwnd, title = _pick()
        if hwnd:
            last_title = title
            # Bare *product* title (e.g. "SmartGis") is often a short-lived
            # splash before the MDI EDIT frame. Specific suite titles
            # (legacy-scene3d-showcase, …) are the real shell — accept them.
            title_l = title.strip().lower()
            bare_splash = bool(needle) and title_l == needle and needle in (
                "smartgis",
                "smartgis views",
                "smartgisviews",
            )
            pause = settle * (2.5 if bare_splash else 1.0)
            time.sleep(pause)
            if not user32.IsWindow(int(hwnd)) or window_area(int(hwnd)) < int(
                min_area
            ):
                continue
            # If still bare splash after extra settle, try once more for EDIT.
            if bare_splash:
                edit_hwnd, edit_title = find_top_level_hwnd_for_pid(
                    pid,
                    timeout_sec=0.5,
                    min_area=min_area,
                    title_substr="EDIT",
                )
                if edit_hwnd:
                    return int(edit_hwnd), edit_title
                # Splash still showing — keep polling for Edit.
                continue
            return int(hwnd), title or last_title
        time.sleep(0.15)
    return 0, last_title


# Child classes that are chrome/chrome scaffolding, not the map view client.
_MAP_CLIENT_SKIP_CLASS = (
    "mdiclient",
    "toolbarwindow32",
    "msctls_statusbar32",
    "rebarwindow32",
    "systabcontrol32",
    "button",
    "static",
    "edit",
    "combobox",
    "listbox",
    "scrollbar",
    "atl:",
    "bcgp",
)


def _is_map_client_candidate(hwnd: int) -> bool:
    if not hwnd or not user32.IsWindow(hwnd) or not user32.IsWindowVisible(hwnd):
        return False
    cls = class_name(hwnd).lower()
    if not cls:
        return False
    for skip in _MAP_CLIENT_SKIP_CLASS:
        if skip in cls:
            return False
    # Tiny dock strips / icons — not a map surface.
    return window_area(hwnd) >= 80_000


def find_map_client_hwnd(
    root_hwnd: int, *, timeout_sec: float = 20.0
) -> tuple[int, str]:
    """Best-effort map/view client under a top-level shell HWND.

    Prefers MFC ``AfxFrameOrView*`` (CView) with the largest on-screen area,
    then any large non-chrome descendant. Returns ``(0, reason)`` when none
    found within timeout (caller should fall back to |root_hwnd|).
    """
    if not root_hwnd or not user32.IsWindow(root_hwnd):
        return 0, "invalid_root"
    deadline = time.time() + max(0.5, float(timeout_sec))
    last_reason = "no_candidate"
    enum_t = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    while time.time() < deadline:
        views: list[tuple[int, int]] = []
        others: list[tuple[int, int]] = []
        keep: list[Any] = []
        seen: set[int] = set()
        queue: list[int] = [int(root_hwnd)]
        while queue:
            cur = queue.pop(0)
            if cur in seen or not user32.IsWindow(cur):
                continue
            seen.add(cur)
            if cur != int(root_hwnd) and _is_map_client_candidate(cur):
                area = window_area(cur)
                cls_l = class_name(cur).lower()
                if "afxframeorview" in cls_l:
                    views.append((cur, area))
                else:
                    others.append((cur, area))

            @enum_t
            def _enum(ch: int, _lp: int, _q=queue) -> bool:  # type: ignore[misc]
                _q.append(int(ch))
                return True

            keep.append(_enum)
            user32.EnumChildWindows(cur, _enum, 0)

        if views:
            views.sort(key=lambda item: item[1], reverse=True)
            return views[0][0], "afxframeorview"
        if others:
            others.sort(key=lambda item: item[1], reverse=True)
            return others[0][0], "largest_child"
        last_reason = "waiting_child"
        time.sleep(0.25)
    return 0, last_reason
