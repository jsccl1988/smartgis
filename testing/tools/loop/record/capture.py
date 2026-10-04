# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""HWND pixel capture (BitBlt / PrintWindow) and present-frame quality gates."""

from __future__ import annotations

import ctypes
import time
from pathlib import Path

from .win32 import (
    BI_RGB,
    BITMAPINFOHEADER,
    DIB_RGB_COLORS,
    PW_RENDERFULLCONTENT,
    SRCCOPY,
    gdi32,
    user32,
)
from .window import (
    bring_hwnd_to_front,
    drop_topmost,
    rect_fully_on_primary,
    window_rect,
)


def write_bmp_bgr24(path: Path, width: int, height: int, bgr: bytes) -> None:
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


_write_bmp_bgr24 = write_bmp_bgr24


def near_black_frac(bgr: bytes, w: int, h: int) -> float:
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


_near_black_frac = near_black_frac


def top_band_chrome_bleed_frac(
    bgr: bytes,
    w: int,
    h: int,
    *,
    band_frac: float = 0.22,
) -> float:
    """Fraction of top-band pixels matching Views TabStrip accent RGB(0,122,204).

    Tight match — Scene3D sky/ocean blues must not count as chrome bleed.
    """
    if w <= 0 or h <= 0 or len(bgr) < w * h * 3:
        return 1.0
    band_h = max(1, int(h * max(0.05, min(0.5, band_frac))))
    accent = total = 0
    step = max(1, (w * band_h) // 4000)
    for y in range(0, band_h):
        row = y * w * 3
        for x in range(0, w, step):
            o = row + x * 3
            if o + 2 >= len(bgr):
                break
            b, g, r = bgr[o], bgr[o + 1], bgr[o + 2]
            total += 1
            if abs(r - 0) < 12 and abs(g - 122) < 28 and abs(b - 204) < 28:
                accent += 1
    return accent / max(1, total)


def tab_accent_bar_bottom(
    bgr: bytes,
    w: int,
    h: int,
    *,
    max_scan_frac: float = 0.35,
) -> int:
    """Y just below the first top Views accent bar, or 0 if none.

    Present HWND can overlap the Map/Data/3D underline under DPI chrome.
    Stops after the first contiguous accent band so sky/content is not cropped.
    """
    if w <= 0 or h <= 0 or len(bgr) < w * h * 3:
        return 0
    scan_h = max(1, int(h * max(0.05, min(0.5, max_scan_frac))))
    best_y = -1
    seen = False
    gap = 0
    for y in range(scan_h):
        row = y * w * 3
        hit = 0
        for x in range(0, w, max(1, w // 200)):
            o = row + x * 3
            if o + 2 >= len(bgr):
                break
            b, g, r = bgr[o], bgr[o + 1], bgr[o + 2]
            if abs(r - 0) < 12 and abs(g - 122) < 28 and abs(b - 204) < 28:
                hit += 1
        if hit >= 25:
            seen = True
            gap = 0
            best_y = y
        elif seen:
            gap += 1
            if gap >= 3:
                break
    return best_y + 2 if best_y >= 0 else 0


def crop_bgr_top(bgr: bytes, w: int, h: int, top: int) -> tuple[bytes, int, int]:
    """Drop the top |top| rows from a tightly packed BGR24 buffer."""
    top = max(0, min(h - 1, int(top)))
    if top <= 0:
        return bgr, w, h
    new_h = h - top
    row = w * 3
    return bgr[top * row : h * row], w, new_h


_crop_bgr_top = crop_bgr_top


def ocean_clear_frac(bgr: bytes, w: int, h: int) -> float:
    """Fraction matching Map2d GDI ocean clear RGB(170,211,223)."""
    if w <= 0 or h <= 0 or len(bgr) < w * h * 3:
        return 1.0
    n = w * h
    step = max(1, n // 4000)
    hit = samples = 0
    for i in range(0, n, step):
        o = i * 3
        if o + 2 >= len(bgr):
            break
        b, g, r = bgr[o], bgr[o + 1], bgr[o + 2]
        samples += 1
        if abs(r - 170) < 18 and abs(g - 211) < 18 and abs(b - 223) < 18:
            hit += 1
    return hit / max(1, samples)


def _commit_bmp(
    path: Path,
    width: int,
    height: int,
    bgr: bytes,
    *,
    method: str,
    near_black: float,
    extra: str = "",
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    write_bmp_bgr24(path, width, height, bgr)
    note = f"{method} near_black={near_black:.3f}"
    if extra:
        note = f"{note} {extra}"
    try:
        (path.with_suffix(path.suffix + ".method.txt")).write_text(
            note, encoding="utf-8"
        )
    except OSError:
        pass


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


def _with_compat_bitmap(
    w: int, h: int, paint
) -> bytes | None:
    """Allocate screen-compatible DC/bitmap, run |paint|, return DIB bytes."""
    hdc_screen = user32.GetDC(0)
    if not hdc_screen:
        return None
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    ok = bool(paint(hdc_mem))
    bgr = _dib_from_hdc(hdc_mem, hbmp, w, h) if ok else None
    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)
    return bgr


def _capture_bitblt(hwnd: int, w: int, h: int, left: int, top: int) -> bytes | None:
    """Copy composited desktop pixels at the HWND rect (virtual-desktop coords)."""
    del hwnd  # rect is absolute; hwnd kept for call-site symmetry
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


def _capture_client_bitblt(hwnd: int, w: int, h: int) -> bytes | None:
    """BitBlt from the HWND client DC (GDI SoT; ignores IDE desktop occlusion).

    MapViewport + FORCE_GDI paints into the client DC. Desktop BitBlt of the
    same screen rect often captures Cursor/IDE chrome above the shell and
    freezes motion_gate. DXGI/FlyCube client DCs are usually black — callers
    fall through to desktop BitBlt / PrintWindow.
    """
    if not hwnd or w <= 0 or h <= 0:
        return None
    hdc_win = user32.GetDC(int(hwnd))
    if not hdc_win:
        return None
    hdc_mem = gdi32.CreateCompatibleDC(hdc_win)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_win, w, h)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    ok = bool(gdi32.BitBlt(hdc_mem, 0, 0, w, h, hdc_win, 0, 0, SRCCOPY))
    bgr = _dib_from_hdc(hdc_mem, hbmp, w, h) if ok else None
    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(int(hwnd), hdc_win)
    return bgr


def _capture_printwindow(hwnd: int, w: int, h: int) -> bytes | None:
    def _paint(hdc_mem: int) -> bool:
        ok = bool(user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT))
        if not ok:
            ok = bool(user32.PrintWindow(hwnd, hdc_mem, 0))
        return ok

    return _with_compat_bitmap(w, h, _paint)


def capture_hwnd_bmp(hwnd: int, path: Path) -> bool:
    """Capture HWND pixels. Prefer BitBlt (multi-mon + GL/D3D); PrintWindow fallback."""
    ok, _frac = capture_hwnd_bmp_ex(hwnd, path)
    return ok


def capture_hwnd_resilient(
    hwnd: int,
    path: Path,
    *,
    clear_existing: bool = False,
    settle_sec: float = 0.15,
    retry_sleep_sec: float = 0.2,
) -> dict:
    """Primary-aware HWND capture with BitBlt retry when near-black.

    On primary: BitBlt first (live GDI). Off-primary: PrintWindow first, then
    always BitBlt-retry when near-black (IDE occlusion / PrintWindow black).
    Stays TOPMOST during capture, then clears it.
    """
    path = Path(path)
    if clear_existing:
        try:
            path.unlink(missing_ok=True)
            path.with_suffix(path.suffix + ".method.txt").unlink(missing_ok=True)
        except OSError:
            pass
    bring_hwnd_to_front(int(hwnd), stay_topmost=True)
    if settle_sec > 0:
        time.sleep(settle_sec)
    left, top, right, bottom = window_rect(int(hwnd))
    on_primary = rect_fully_on_primary(left, top, right, bottom)
    ok, frac = capture_hwnd_bmp_ex(
        int(hwnd), path, prefer_printwindow=not on_primary
    )
    if not ok or frac > 0.90:
        if retry_sleep_sec > 0:
            time.sleep(retry_sleep_sec)
        bring_hwnd_to_front(int(hwnd), stay_topmost=True)
        ok2, frac2 = capture_hwnd_bmp_ex(
            int(hwnd), path, prefer_printwindow=False
        )
        if ok2 and frac2 < frac:
            ok, frac = ok2, frac2
    bring_hwnd_to_front(int(hwnd), stay_topmost=False)
    return {
        "ok": bool(ok) and float(frac) < 0.90,
        "path": str(path),
        "near_black": round(float(frac), 4),
        "hwnd": int(hwnd),
        "on_primary": bool(on_primary),
    }


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
    left, top, right, bottom = window_rect(hwnd)
    w = max(1, right - left)
    h = max(1, bottom - top)

    candidates: list[tuple[str, bytes]] = []

    def _try_accept(method: str, bgr: bytes, *, early: bool) -> tuple[bool, float] | None:
        frac = near_black_frac(bgr, w, h)
        candidates.append((method, bgr))
        if early and frac < 0.90:
            _commit_bmp(path, w, h, bgr, method=method, near_black=frac)
            return True, frac
        return None

    if prefer_printwindow:
        pw = _capture_printwindow(hwnd, w, h)
        if pw is not None:
            hit = _try_accept("printwindow", pw, early=True)
            if hit is not None:
                return hit

    # GDI map client: window DC beats desktop BitBlt when IDE covers the rect.
    client = _capture_client_bitblt(hwnd, w, h)
    if client is not None:
        # Reject empty/DXGI-black client buffers so FlyCube can use desktop.
        cfrac = near_black_frac(client, w, h)
        if cfrac < 0.90:
            hit = _try_accept(
                "client_bitblt", client, early=not prefer_printwindow
            )
            if hit is not None:
                return hit

    bitblt = _capture_bitblt(hwnd, w, h, left, top)
    if bitblt is not None:
        # Fast path: desktop BitBlt (dual-mon + DXGI present pixels).
        hit = _try_accept("bitblt", bitblt, early=not prefer_printwindow)
        if hit is not None:
            return hit

    if not prefer_printwindow:
        pw = _capture_printwindow(hwnd, w, h)
        if pw is not None:
            candidates.append(("printwindow", pw))

    if not candidates:
        return False, 1.0

    # Prefer less-black, then prefer client_bitblt over desktop IDE bleed.
    def _rank(item: tuple[str, bytes]) -> tuple[float, int]:
        method, buf = item
        pref = 0 if method == "client_bitblt" else (1 if method == "printwindow" else 2)
        return near_black_frac(buf, w, h), pref

    method, bgr = min(candidates, key=_rank)
    frac = near_black_frac(bgr, w, h)
    _commit_bmp(path, w, h, bgr, method=method, near_black=frac)
    return True, frac


def capture_flycube_present_bmp(
    hwnd: int,
    path: Path,
    *,
    attempts: int = 5,
    max_chrome_bleed: float = 0.015,
    max_ocean_clear: float = 0.55,
) -> tuple[bool, float, dict]:
    """BitBlt FlyCube Present without covering it with a TOPMOST shell.

    Crops an overlapping Views TabStrip accent bar before scoring.
    """
    meta: dict = {
        "attempts": 0,
        "chrome_bleed": None,
        "ocean_clear": None,
        "crop_top": 0,
        "method": "",
        "reject": "",
    }
    if not hwnd or not user32.IsWindow(hwnd):
        meta["reject"] = "invalid_hwnd"
        return False, 1.0, meta

    path = Path(path)
    best: tuple[float, float, float, bytes, int, int, int] | None = None
    for attempt in range(max(1, int(attempts))):
        meta["attempts"] = attempt + 1
        bring_hwnd_to_front(int(hwnd), stay_topmost=True, restore=False)
        time.sleep(0.25 + 0.15 * attempt)
        left, top, right, bottom = window_rect(int(hwnd))
        w = max(1, right - left)
        h = max(1, bottom - top)
        bitblt = _capture_bitblt(int(hwnd), w, h, left, top)
        if bitblt is None:
            continue
        crop = tab_accent_bar_bottom(bitblt, w, h)
        # Present popup can overlap Map|Data|3D underline under DPI chrome;
        # keep cropping while the top band still looks like TabStrip accent.
        total_crop = 0
        for _ in range(4):
            if crop <= 0:
                break
            bitblt, w, h = crop_bgr_top(bitblt, w, h, crop)
            total_crop += crop
            if h < 240:
                break
            crop = tab_accent_bar_bottom(bitblt, w, h)
        crop = total_crop
        bleed = top_band_chrome_bleed_frac(bitblt, w, h)
        ocean = ocean_clear_frac(bitblt, w, h)
        black = near_black_frac(bitblt, w, h)
        # Near-white hollow Present (cleared swapchain) — not carto gold.
        white = 0.0
        if bitblt and w > 0 and h > 0:
            step = max(1, (w * h) // 8000)
            n_s = 0
            n_w = 0
            for i in range(0, len(bitblt) - 2, 3 * step):
                b, g, r = bitblt[i], bitblt[i + 1], bitblt[i + 2]
                n_s += 1
                if r > 230 and g > 230 and b > 210:
                    n_w += 1
            white = n_w / max(1, n_s)
        meta["chrome_bleed"] = round(bleed, 4)
        meta["ocean_clear"] = round(ocean, 4)
        meta["near_white"] = round(white, 4)
        meta["crop_top"] = int(crop)
        cand = (bleed, ocean, black + white, bitblt, w, h, crop, white)
        if best is None or cand[:3] < best[:3]:
            best = cand
        # Reject near-black DXGI (china seed / first present still settling).
        # China light-land wash often has white~0.6 with ocean coastline — that
        # is carto gold, not a cleared swapchain (score_views_present_dxgi).
        coastline = (
            ocean > 0.08
            and white > 0.20
            and white < 0.85
            and black < 0.08
            and ocean <= max_ocean_clear
        )
        if (
            bleed <= max_chrome_bleed
            and ocean <= max_ocean_clear
            and black < 0.45
            and (white < 0.45 or coastline)
        ):
            break
        time.sleep(0.35)

    drop_topmost(int(hwnd))
    if best is None:
        meta["reject"] = "bitblt_failed"
        return False, 1.0, meta

    bleed, ocean, black, bgr, w, h, crop, white = best
    meta["chrome_bleed"] = round(bleed, 4)
    meta["ocean_clear"] = round(ocean, 4)
    meta["near_white"] = round(white, 4)
    meta["crop_top"] = int(crop)
    meta["method"] = f"bitblt near_black={black:.3f} near_white={white:.3f}"
    _commit_bmp(
        path,
        w,
        h,
        bgr,
        method="bitblt",
        near_black=black,
        extra=(
            f"chrome_bleed={bleed:.3f} ocean={ocean:.3f} "
            f"near_white={white:.3f} crop_top={crop}"
        ),
    )

    if bleed > max_chrome_bleed:
        meta["reject"] = "chrome_bleed"
        return False, black, meta
    if ocean > max_ocean_clear:
        meta["reject"] = "ocean_clear_shell_hole"
        return False, black, meta
    if black >= 0.92:
        meta["reject"] = "near_black"
        return False, black, meta
    # Hollow cleared swapchain is near-uniform white. Light china land wash
    # can exceed 0.55 white while still having gray water / point markers —
    # defer that distinction to score_views_present_dxgi (landish/buckets).
    if white >= 0.80:
        meta["reject"] = "near_white_hollow"
        return False, black, meta
    return True, black, meta

