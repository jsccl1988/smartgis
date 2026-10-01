# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Window recorder sidecar: multi-monitor safe HWND capture + optional ffmpeg."""

from __future__ import annotations

import ctypes
import shutil
import subprocess
import sys
import time
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
user32.FindWindowW.restype = wintypes.HWND
user32.EnumWindows.argtypes = [
    ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM),
    wintypes.LPARAM,
]
user32.EnumWindows.restype = wintypes.BOOL
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowVisible.restype = wintypes.BOOL
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user32.GetWindowRect.restype = wintypes.BOOL
user32.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
user32.PrintWindow.restype = wintypes.BOOL
user32.GetDC.argtypes = [wintypes.HWND]
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
user32.ReleaseDC.restype = ctypes.c_int
user32.IsWindow.argtypes = [wintypes.HWND]
user32.IsWindow.restype = wintypes.BOOL
user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetClassNameW.restype = ctypes.c_int
user32.GetWindowThreadProcessId.argtypes = [
    wintypes.HWND,
    ctypes.POINTER(wintypes.DWORD),
]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.EnumChildWindows.argtypes = [
    wintypes.HWND,
    ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM),
    wintypes.LPARAM,
]
user32.EnumChildWindows.restype = wintypes.BOOL
user32.GetSystemMetrics.argtypes = [ctypes.c_int]
user32.GetSystemMetrics.restype = ctypes.c_int
user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
user32.ShowWindow.restype = wintypes.BOOL
user32.SetForegroundWindow.argtypes = [wintypes.HWND]
user32.SetForegroundWindow.restype = wintypes.BOOL

gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = wintypes.HBITMAP
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteObject.restype = wintypes.BOOL
gdi32.DeleteDC.argtypes = [wintypes.HDC]
gdi32.DeleteDC.restype = wintypes.BOOL
gdi32.BitBlt.argtypes = [
    wintypes.HDC,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.HDC,
    ctypes.c_int,
    ctypes.c_int,
    wintypes.DWORD,
]
gdi32.BitBlt.restype = wintypes.BOOL
gdi32.GetDIBits.argtypes = [
    wintypes.HDC,
    wintypes.HBITMAP,
    wintypes.UINT,
    wintypes.UINT,
    ctypes.c_void_p,
    ctypes.c_void_p,
    wintypes.UINT,
]
gdi32.GetDIBits.restype = ctypes.c_int

PW_RENDERFULLCONTENT = 0x00000002
BI_RGB = 0
DIB_RGB_COLORS = 0
SRCCOPY = 0x00CC0020
SM_XVIRTUALSCREEN = 76
SM_YVIRTUALSCREEN = 77
SM_CXVIRTUALSCREEN = 78
SM_CYVIRTUALSCREEN = 79
SM_CXSCREEN = 0
SM_CYSCREEN = 1


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


def _window_area(hwnd: int) -> int:
    left, top, right, bottom = _window_rect(hwnd)
    return max(0, right - left) * max(0, bottom - top)


def hwnd_pid(hwnd: int) -> int:
    """Owning process id for |hwnd|, or 0 when unavailable."""
    if not hwnd or not user32.IsWindow(hwnd):
        return 0
    pid = wintypes.DWORD(0)
    user32.GetWindowThreadProcessId(int(hwnd), ctypes.byref(pid))
    return int(pid.value)


def is_window(hwnd: int) -> bool:
    return bool(hwnd) and bool(user32.IsWindow(int(hwnd)))


def bring_hwnd_to_front(hwnd: int) -> bool:
    """Restore + foreground so BitBlt is not occluded by IDE chrome."""
    if not hwnd or not user32.IsWindow(hwnd):
        return False
    user32.ShowWindow(int(hwnd), 9)  # SW_RESTORE
    user32.ShowWindow(int(hwnd), 5)  # SW_SHOW
    return bool(user32.SetForegroundWindow(int(hwnd)))


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
    found: list[tuple[int, str]] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def _enum(hwnd: int, _lp: int) -> bool:  # type: ignore[misc]
        if not user32.IsWindowVisible(hwnd):
            return True
        if want_pid is not None and hwnd_pid(int(hwnd)) != want_pid:
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        if n <= 0:
            return True
        buf = ctypes.create_unicode_buffer(n + 1)
        user32.GetWindowTextW(hwnd, buf, n + 1)
        title = buf.value or ""
        if needle.lower() in title.lower():
            found.append((int(hwnd), title))
        return True

    while time.time() < deadline:
        found.clear()
        user32.EnumWindows(_enum, 0)
        if found:
            found.sort(key=lambda item: _window_area(item[0]), reverse=True)
            return found[0]
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
    found: list[tuple[int, str]] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def _enum(hwnd: int, _lp: int) -> bool:  # type: ignore[misc]
        if not user32.IsWindowVisible(hwnd):
            return True
        if hwnd_pid(int(hwnd)) != want:
            return True
        area = _window_area(int(hwnd))
        if area < int(min_area):
            return True
        n = user32.GetWindowTextLengthW(hwnd)
        title = ""
        if n > 0:
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            title = buf.value or ""
        if needle and needle not in title.lower():
            return True
        found.append((int(hwnd), title))
        return True

    while time.time() < deadline:
        found.clear()
        user32.EnumWindows(_enum, 0)
        if found:
            found.sort(key=lambda item: _window_area(item[0]), reverse=True)
            return found[0]
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
            if not user32.IsWindow(int(hwnd)) or _window_area(int(hwnd)) < int(
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


def _class_name(hwnd: int) -> str:
    buf = ctypes.create_unicode_buffer(256)
    n = user32.GetClassNameW(hwnd, buf, 256)
    if n <= 0:
        return ""
    return buf.value or ""


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
    cls = _class_name(hwnd).lower()
    if not cls:
        return False
    for skip in _MAP_CLIENT_SKIP_CLASS:
        if skip in cls:
            return False
    area = _window_area(hwnd)
    # Tiny dock strips / icons — not a map surface.
    return area >= 80_000


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
                area = _window_area(cur)
                cls_l = _class_name(cur).lower()
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


def record_mode_pref(env: dict[str, str] | None = None) -> str:
    """auto | bmp | ffmpeg — dual-mon default stays auto (title grab → bmp)."""
    import os

    src = env if env is not None else os.environ
    v = str(src.get("SMT_HARNESS_RECORD_MODE", "auto")).strip().lower()
    if v in ("bmp", "bmp_burst", "burst"):
        return "bmp"
    if v in ("ffmpeg", "mp4", "video"):
        return "ffmpeg"
    return "auto"


def _which_ffmpeg() -> str | None:
    return shutil.which("ffmpeg")


def _window_rect(hwnd: int) -> tuple[int, int, int, int]:
    rc = wintypes.RECT()
    if not user32.GetWindowRect(hwnd, ctypes.byref(rc)):
        return 0, 0, 0, 0
    return int(rc.left), int(rc.top), int(rc.right), int(rc.bottom)


def _virtual_screen() -> dict[str, int]:
    return {
        "origin_x": int(user32.GetSystemMetrics(SM_XVIRTUALSCREEN)),
        "origin_y": int(user32.GetSystemMetrics(SM_YVIRTUALSCREEN)),
        "width": int(user32.GetSystemMetrics(SM_CXVIRTUALSCREEN)),
        "height": int(user32.GetSystemMetrics(SM_CYVIRTUALSCREEN)),
        "primary_w": int(user32.GetSystemMetrics(SM_CXSCREEN)),
        "primary_h": int(user32.GetSystemMetrics(SM_CYSCREEN)),
    }


def _rect_fully_on_primary(left: int, top: int, right: int, bottom: int) -> bool:
    """True when the HWND lies entirely inside the primary monitor (0,0)-(pw,ph)."""
    pw = int(user32.GetSystemMetrics(SM_CXSCREEN))
    ph = int(user32.GetSystemMetrics(SM_CYSCREEN))
    return left >= 0 and top >= 0 and right <= pw and bottom <= ph and right > left and bottom > top


def _write_bmp_bgr24(path: Path, width: int, height: int, bgr: bytes) -> None:
    row = ((width * 3 + 3) // 4) * 4
    pad = row - width * 3
    pixel_size = row * height
    header = bytearray(14 + 40)
    header[0:2] = b"BM"
    ctypes.c_uint32.from_buffer(header, 2).value = 14 + 40 + pixel_size
    ctypes.c_uint32.from_buffer(header, 10).value = 14 + 40
    ctypes.c_uint32.from_buffer(header, 14).value = 40
    ctypes.c_int32.from_buffer(header, 18).value = width
    ctypes.c_int32.from_buffer(header, 22).value = -height  # top-down
    ctypes.c_uint16.from_buffer(header, 26).value = 1
    ctypes.c_uint16.from_buffer(header, 28).value = 24
    body = bytearray()
    for y in range(height):
        start = y * width * 3
        body.extend(bgr[start : start + width * 3])
        if pad:
            body.extend(b"\x00" * pad)
    path.write_bytes(bytes(header) + bytes(body))


def _near_black_frac(bgr: bytes, w: int, h: int) -> float:
    n = max(1, w * h)
    step = max(1, n // 4000)
    black = 0
    samples = 0
    for i in range(0, n, step):
        o = i * 3
        if o + 2 >= len(bgr):
            break
        b, g, r = bgr[o], bgr[o + 1], bgr[o + 2]
        samples += 1
        if r < 25 and g < 25 and b < 25:
            black += 1
    return black / max(1, samples)


def _dib_from_hdc(hdc_mem: int, hbmp: int, w: int, h: int) -> bytes | None:
    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 24
    bmi.biCompression = BI_RGB
    buf = (ctypes.c_ubyte * (w * h * 3))()
    got = gdi32.GetDIBits(
        hdc_mem, hbmp, 0, h, ctypes.byref(buf), ctypes.byref(bmi), DIB_RGB_COLORS
    )
    if got == 0:
        return None
    return bytes(buf)


def _capture_bitblt(hwnd: int, w: int, h: int, left: int, top: int) -> bytes | None:
    """Copy composited desktop pixels at the HWND rect (virtual-desktop coords)."""
    hdc_screen = user32.GetDC(0)
    if not hdc_screen:
        return None
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    ok = bool(
        gdi32.BitBlt(hdc_mem, 0, 0, w, h, hdc_screen, left, top, SRCCOPY)
    )
    bgr = _dib_from_hdc(hdc_mem, hbmp, w, h) if ok else None
    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)
    return bgr


def _capture_printwindow(hwnd: int, w: int, h: int) -> bytes | None:
    hdc_screen = user32.GetDC(0)
    if not hdc_screen:
        return None
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    ok = bool(user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT))
    if not ok:
        ok = bool(user32.PrintWindow(hwnd, hdc_mem, 0))
    bgr = _dib_from_hdc(hdc_mem, hbmp, w, h) if ok else None
    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)
    return bgr


def capture_hwnd_bmp(hwnd: int, path: Path) -> bool:
    """Capture HWND pixels. Prefer BitBlt (multi-mon + GL/D3D); PrintWindow fallback."""
    ok, _frac = capture_hwnd_bmp_ex(hwnd, path)
    return ok


def capture_hwnd_bmp_ex(
    hwnd: int,
    path: Path,
    *,
    prefer_printwindow: bool = False,
) -> tuple[bool, float]:
    """Like capture_hwnd_bmp, also returns near-black fraction of the chosen buffer.

    |prefer_printwindow|: for occluded GDI shells (IDE covering Edit), BitBlt
    copies whatever is on top of the HWND rect. Prefer PrintWindow so the
    capture is the real window content regardless of z-order.
    """
    if not hwnd or not user32.IsWindow(hwnd):
        return False, 1.0
    left, top, right, bottom = _window_rect(hwnd)
    w = max(1, right - left)
    h = max(1, bottom - top)

    candidates: list[tuple[str, bytes]] = []
    if prefer_printwindow:
        pw = _capture_printwindow(hwnd, w, h)
        if pw is not None:
            candidates.append(("printwindow", pw))
            frac_pw = _near_black_frac(pw, w, h)
            if frac_pw < 0.90:
                path.parent.mkdir(parents=True, exist_ok=True)
                _write_bmp_bgr24(path, w, h, pw)
                try:
                    (path.with_suffix(path.suffix + ".method.txt")).write_text(
                        f"printwindow near_black={frac_pw:.3f}", encoding="utf-8"
                    )
                except OSError:
                    pass
                return True, frac_pw

    bitblt = _capture_bitblt(hwnd, w, h, left, top)
    if bitblt is not None:
        candidates.append(("bitblt", bitblt))
        # Fast path: BitBlt already has real pixels (dual-mon desktop DC).
        if not prefer_printwindow and _near_black_frac(bitblt, w, h) < 0.90:
            method, bgr = "bitblt", bitblt
            frac = _near_black_frac(bgr, w, h)
            path.parent.mkdir(parents=True, exist_ok=True)
            _write_bmp_bgr24(path, w, h, bgr)
            try:
                (path.with_suffix(path.suffix + ".method.txt")).write_text(
                    f"{method} near_black={frac:.3f}", encoding="utf-8"
                )
            except OSError:
                pass
            return True, frac
    if not prefer_printwindow:
        pw = _capture_printwindow(hwnd, w, h)
        if pw is not None:
            candidates.append(("printwindow", pw))
    if not candidates:
        return False, 1.0

    # Pick the less-black buffer (PrintWindow often returns black for GL/D3D).
    method, bgr = min(
        candidates, key=lambda item: _near_black_frac(item[1], w, h)
    )
    frac = _near_black_frac(bgr, w, h)
    path.parent.mkdir(parents=True, exist_ok=True)
    _write_bmp_bgr24(path, w, h, bgr)
    try:
        (path.with_suffix(path.suffix + ".method.txt")).write_text(
            f"{method} near_black={frac:.3f}", encoding="utf-8"
        )
    except OSError:
        pass
    return True, frac


class HwndRecorder:
    """Start/stop recording of a top-level HWND into captures/record/."""

    def __init__(
        self,
        *,
        captures_dir: Path,
        suite_id: str,
        title_substr: str,
        fps: float = 10.0,
        find_timeout_sec: float = 45.0,
    ) -> None:
        self.captures_dir = Path(captures_dir)
        self.suite_id = suite_id
        self.title_substr = title_substr
        self.fps = max(1.0, float(fps))
        self.find_timeout_sec = find_timeout_sec
        self._proc: subprocess.Popen[Any] | None = None
        self._mode = "none"
        self._path: Path | None = None
        self._frames_dir: Path | None = None
        self._burst_stop = False
        self._burst_thread: Any = None
        self._hwnd = 0
        self._title = ""
        self._started = 0.0
        self._error: str | None = None
        self._rect: tuple[int, int, int, int] | None = None
        self._ffmpeg_skip: str | None = None

    def bind_hwnd(self, hwnd: int, title: str = "") -> None:
        self._hwnd = int(hwnd)
        self._title = title or self.title_substr

    def _try_start_ffmpeg(self, out: Path) -> bool:
        ffmpeg = _which_ffmpeg()
        if not ffmpeg:
            return False
        left, top, right, bottom = _window_rect(self._hwnd)
        self._rect = (left, top, right, bottom)
        w = max(2, right - left)
        h = max(2, bottom - top)
        title = (self._title or "").strip()

        # Prefer window-title grab: multi-monitor safe, no desktop offset math.
        if title:
            cmd = [
                ffmpeg,
                "-y",
                "-f",
                "gdigrab",
                "-framerate",
                str(int(self.fps)),
                "-i",
                f"title={title}",
                "-an",
                "-c:v",
                "libx264",
                "-pix_fmt",
                "yuv420p",
                "-preset",
                "ultrafast",
                str(out),
            ]
            try:
                self._proc = subprocess.Popen(
                    cmd,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                self._path = out
                self._mode = "ffmpeg_title"
                self._started = time.time()
                return True
            except OSError as exc:  # noqa: BLE001
                self._error = f"ffmpeg_title: {exc}"
                self._proc = None

        # desktop + offset only when the HWND is fully on the primary monitor.
        # Dual-monitor: gdigrab -i desktop often covers primary only → wrong crop
        # (secondary HWND at x≈2560+ yields black / wrong region).
        if not _rect_fully_on_primary(left, top, right, bottom):
            self._ffmpeg_skip = "hwnd_off_primary"
            return False

        # gdigrab -i desktop origin is the primary monitor top-left (0,0), not
        # SM_XVIRTUALSCREEN. On-primary GetWindowRect is already primary-relative
        # when the virtual origin is (0,0); negative virtual origins still keep
        # primary windows in [0..pw) x [0..ph).
        ox = left
        oy = top
        cmd = [
            ffmpeg,
            "-y",
            "-f",
            "gdigrab",
            "-framerate",
            str(int(self.fps)),
            "-offset_x",
            str(ox),
            "-offset_y",
            str(oy),
            "-video_size",
            f"{w}x{h}",
            "-i",
            "desktop",
            "-an",
            "-c:v",
            "libx264",
            "-pix_fmt",
            "yuv420p",
            "-preset",
            "ultrafast",
            str(out),
        ]
        try:
            self._proc = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self._path = out
            self._mode = "ffmpeg_desktop"
            self._started = time.time()
            return True
        except OSError as exc:  # noqa: BLE001
            self._error = f"ffmpeg_desktop: {exc}"
            self._proc = None
            return False

    def start_after_hwnd(self, wait_for_hwnd: bool = True) -> dict[str, Any]:
        """Resolve HWND then start ffmpeg or BMP burst. Soft-fail (never raises)."""
        # Nested: captures/record/<suite>_<stamp>.* (keep captures/ root for
        # marks / BMPs / loop reports).
        record_dir = self.captures_dir / "record"
        record_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        leaf = f"{self.suite_id.replace('.', '_')}_{stamp}"
        if wait_for_hwnd and not self._hwnd:
            self._hwnd, self._title = find_window_by_title_substr(
                self.title_substr, timeout_sec=self.find_timeout_sec
            )
        if not self._hwnd:
            self._mode = "skipped"
            self._error = "hwnd_timeout"
            return self.status()

        self._rect = _window_rect(self._hwnd)
        mode_pref = record_mode_pref()
        out_mp4 = record_dir / f"{leaf}.mp4"
        if mode_pref != "bmp" and self._try_start_ffmpeg(out_mp4):
            return self.status()
        if mode_pref == "ffmpeg" and not self._mode.startswith("ffmpeg"):
            # Forced ffmpeg but title/desktop grab failed — still fall to BMP.
            if not self._ffmpeg_skip:
                self._ffmpeg_skip = "ffmpeg_forced_unavailable"

        # BMP burst fallback (~fps): BitBlt-first capture (see capture_hwnd_bmp).
        # Dual-mon safe: uses virtual-desktop BitBlt coords + PrintWindow pick.
        frames = record_dir / f"{leaf}_frames"
        frames.mkdir(parents=True, exist_ok=True)
        self._frames_dir = frames
        self._path = frames
        self._mode = "bmp_burst"
        self._started = time.time()
        self._burst_stop = False

        import threading

        def _burst() -> None:
            i = 0
            good = 0
            interval = 1.0 / self.fps
            while not self._burst_stop:
                t0 = time.time()
                if not user32.IsWindow(self._hwnd):
                    break
                if not user32.IsWindowVisible(self._hwnd):
                    # Startup: HWND may briefly report invisible — wait a bit.
                    if good == 0 and (time.time() - self._started) < 8.0:
                        time.sleep(interval)
                        continue
                    break
                path = frames / f"frame_{i:05d}.bmp"
                # Prefer PrintWindow: BitBlt of DXGI flip present/TOPMOST shell
                # often yields sheared chrome or near-black on dual-mon harness.
                ok, near_black = capture_hwnd_bmp_ex(
                    self._hwnd, path, prefer_printwindow=True
                )
                if not ok:
                    if good == 0 and (time.time() - self._started) < 8.0:
                        time.sleep(interval)
                        continue
                    break
                # Startup navy / flip-model black: skip until a real frame.
                # Teardown black: stop only after we already had good frames.
                if near_black > 0.98:
                    try:
                        path.unlink(missing_ok=True)
                        path.with_suffix(path.suffix + ".method.txt").unlink(
                            missing_ok=True
                        )
                    except OSError:
                        pass
                    if good > 0:
                        break
                    time.sleep(interval)
                    continue
                good += 1
                i += 1
                elapsed = time.time() - t0
                time.sleep(max(0.0, interval - elapsed))

        self._burst_thread = threading.Thread(target=_burst, daemon=True)
        self._burst_thread.start()
        return self.status()

    def stop(self) -> dict[str, Any]:
        if self._mode.startswith("ffmpeg") and self._proc is not None:
            try:
                if self._proc.stdin:
                    self._proc.stdin.write(b"q")
                    self._proc.stdin.flush()
                self._proc.wait(timeout=8)
            except Exception:  # noqa: BLE001
                try:
                    self._proc.kill()
                except Exception:  # noqa: BLE001
                    pass
            self._proc = None
        if self._mode == "bmp_burst":
            self._burst_stop = True
            if self._burst_thread is not None:
                self._burst_thread.join(timeout=5.0)
                self._burst_thread = None
        return self.status()

    def status(self) -> dict[str, Any]:
        out: dict[str, Any] = {
            "mode": self._mode,
            "record_mode_pref": record_mode_pref(),
            "title_substr": self.title_substr,
            "hwnd": self._hwnd,
            "title": self._title,
            "virtual_screen": _virtual_screen(),
        }
        if self._rect is not None:
            left, top, right, bottom = self._rect
            out["rect"] = {
                "left": left,
                "top": top,
                "right": right,
                "bottom": bottom,
                "w": max(0, right - left),
                "h": max(0, bottom - top),
                "on_primary": _rect_fully_on_primary(left, top, right, bottom),
            }
        if self._path is not None:
            out["record_path"] = str(self._path)
            if self._path.is_file():
                out["bytes"] = self._path.stat().st_size
            elif self._path.is_dir():
                out["frame_count"] = sum(1 for _ in self._path.glob("frame_*.bmp"))
        if self._started:
            out["duration_s"] = round(time.time() - self._started, 3)
        if self._error:
            out["error"] = self._error
        if self._ffmpeg_skip:
            out["ffmpeg_skip"] = self._ffmpeg_skip
        return out


def record_enabled(env: dict[str, str] | None = None) -> bool:
    import os

    src = env if env is not None else os.environ
    v = str(src.get("SMT_HARNESS_RECORD", "")).strip().lower()
    return v in ("1", "true", "yes", "on")


def main(argv: list[str] | None = None) -> int:
    import argparse

    p = argparse.ArgumentParser(description="Record HWND by title substring")
    p.add_argument("--title", required=True, help="Window title substring")
    p.add_argument("--out", type=Path, required=True, help="captures dir")
    p.add_argument("--suite", default="record_smoke")
    p.add_argument("--sec", type=float, default=3.0)
    p.add_argument("--fps", type=float, default=10.0)
    args = p.parse_args(argv)
    rec = HwndRecorder(
        captures_dir=args.out,
        suite_id=args.suite,
        title_substr=args.title,
        fps=args.fps,
    )
    print(json_dumps(rec.start_after_hwnd()), flush=True)
    time.sleep(max(0.5, args.sec))
    print(json_dumps(rec.stop()), flush=True)
    st = rec.status()
    if st.get("mode") == "skipped":
        return 2
    return 0


def json_dumps(obj: Any) -> str:
    import json

    return json.dumps(obj, indent=2)


if __name__ == "__main__":
    sys.exit(main())
