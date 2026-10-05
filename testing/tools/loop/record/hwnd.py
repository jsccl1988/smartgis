# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Window recorder sidecar: multi-monitor safe HWND capture + optional ffmpeg.

Public facade — implementations live in composed modules:
  win32.py / window.py / capture.py / ffmpeg.py / session.py
"""

from __future__ import annotations

import sys

from .capture import (  # noqa: F401
    _capture_bitblt,
    _capture_printwindow,
    _crop_bgr_top,
    _dib_from_hdc,
    _near_black_frac,
    _write_bmp_bgr24,
    capture_flycube_present_bmp,
    capture_hwnd_bmp,
    capture_hwnd_bmp_ex,
    capture_hwnd_resilient,
    near_black_frac,
    ocean_clear_frac,
    tab_accent_bar_bottom,
    top_band_chrome_bleed_frac,
    write_bmp_bgr24,
)
from .ffmpeg import (  # noqa: F401
    _encode_frames_mp4,
    _which_ffmpeg,
    encode_frames_mp4,
    which_ffmpeg,
)
from .session import (  # noqa: F401
    HwndRecorder,
    json_dumps,
    main,
    record_enabled,
    record_mode_pref,
)
from .win32 import (  # noqa: F401
    BI_RGB,
    BITMAPINFOHEADER,
    DIB_RGB_COLORS,
    PW_RENDERFULLCONTENT,
    SM_CXSCREEN,
    SM_CXVIRTUALSCREEN,
    SM_CYSCREEN,
    SM_CYVIRTUALSCREEN,
    SM_XVIRTUALSCREEN,
    SM_YVIRTUALSCREEN,
    SRCCOPY,
    gdi32,
    kernel32,
    user32,
)
from .window import (  # noqa: F401
    _class_name,
    _rect_fully_on_primary,
    _virtual_screen,
    _window_area,
    _window_rect,
    bring_hwnd_to_front,
    class_name,
    drop_topmost,
    find_map_client_hwnd,
    find_top_level_hwnd_for_pid,
    find_window_by_title_substr,
    hwnd_pid,
    is_window,
    rect_fully_on_primary,
    virtual_screen,
    wait_stable_shell_hwnd,
    window_area,
    window_rect,
    client_rect,
)

if __name__ == "__main__":
    sys.exit(main())
