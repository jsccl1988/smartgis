# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""ffmpeg discovery and frame-directory → mp4 encode helpers."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


def which_ffmpeg() -> str | None:
    found = shutil.which("ffmpeg")
    if found:
        return found
    # Common Windows installs when PATH is thin (agent shells).
    for candidate in (
        Path(r"C:\ffmpeg\bin\ffmpeg.exe"),
        Path(r"C:\ProgramData\chocolatey\bin\ffmpeg.exe"),
        Path.home() / "scoop" / "apps" / "ffmpeg" / "current" / "bin" / "ffmpeg.exe",
    ):
        if candidate.is_file():
            return str(candidate)
    return None


_which_ffmpeg = which_ffmpeg


def encode_frames_mp4(frames_dir: Path | None, *, fps: float) -> Path | None:
    """Encode ``frame_*.bmp`` → sibling ``.mp4``. Soft-fail when ffmpeg missing."""
    if frames_dir is None or not frames_dir.is_dir():
        return None
    frames = sorted(frames_dir.glob("frame_*.bmp"))
    if len(frames) < 2:
        return None
    ff = which_ffmpeg()
    if not ff:
        return None
    out = frames_dir.with_name(frames_dir.name.replace("_frames", "") + ".mp4")
    # pattern frame_%05d.bmp
    pattern = str(frames_dir / "frame_%05d.bmp")
    cmd = [
        ff,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-framerate",
        str(max(1.0, float(fps))),
        "-i",
        pattern,
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-preset",
        "ultrafast",
        str(out),
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
    except OSError:
        return None
    if proc.returncode != 0 or not out.is_file() or out.stat().st_size < 1000:
        return None
    return out


_encode_frames_mp4 = encode_frames_mp4
