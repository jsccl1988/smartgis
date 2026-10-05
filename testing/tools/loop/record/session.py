# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""HWND recording session: ffmpeg title/desktop grab or BMP burst fallback."""

from __future__ import annotations

import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .capture import capture_hwnd_bmp_ex
from .ffmpeg import encode_frames_mp4, which_ffmpeg
from .win32 import user32
from .window import (
    bring_hwnd_to_front,
    find_window_by_title_substr,
    is_usable_map_record_hwnd,
    rect_fully_on_primary,
    resolve_map_record_hwnd,
    virtual_screen,
    wait_stable_shell_hwnd,
    window_area,
    window_rect,
)


def _env_get(src: dict[str, str], *keys: str, default: str = "") -> str:
    for k in keys:
        if k in src and str(src[k]).strip() != "":
            return str(src[k])
    return default


def record_mode_pref(env: dict[str, str] | None = None) -> str:
    """auto | bmp | ffmpeg — dual-mon default stays auto (title grab → bmp)."""
    import os

    src = env if env is not None else os.environ
    v = _env_get(src, "harness-record-mode", "HARNESS_RECORD_MODE", default="auto").strip().lower()
    if v in ("bmp", "bmp_burst", "burst"):
        return "bmp"
    if v in ("ffmpeg", "mp4", "video"):
        return "ffmpeg"
    return "auto"


def record_enabled(env: dict[str, str] | None = None) -> bool:
    import os

    src = env if env is not None else os.environ
    v = _env_get(src, "harness-record", "HARNESS_RECORD").strip().lower()
    return v in ("1", "true", "yes", "on")


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
        env: dict[str, str] | None = None,
    ) -> None:
        self.captures_dir = Path(captures_dir)
        self.suite_id = suite_id
        self.title_substr = title_substr
        self.fps = max(1.0, float(fps))
        self.find_timeout_sec = find_timeout_sec
        # Suite env (HARNESS_RECORD_MODE=bmp) — do not fall back to parent
        # process os.environ alone (loop_runner reports mode=auto otherwise).
        self._env = dict(env) if env is not None else None
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
        self._mp4_path: Path | None = None
        self._target_pid: int = 0

    def bind_target_pid(self, pid: int) -> None:
        """Restrict HWND lookup to this process (inproc loop_runner child)."""
        self._target_pid = int(pid)

    def bind_hwnd(self, hwnd: int, title: str = "") -> None:
        self._hwnd = int(hwnd)
        self._title = title or self.title_substr

    def _try_start_ffmpeg(self, out: Path) -> bool:
        ffmpeg = which_ffmpeg()
        if not ffmpeg:
            return False
        left, top, right, bottom = window_rect(self._hwnd)
        self._rect = (left, top, right, bottom)
        w = max(2, right - left)
        h = max(2, bottom - top)
        title = (self._title or "").strip()
        # gdigrab -video_size is fixed at start; window(resize) needs bmp_burst.

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
        if not rect_fully_on_primary(left, top, right, bottom):
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
            pid = int(self._target_pid) if self._target_pid else None
            if pid:
                # Bind the launched PE only — title substr matches Cursor too.
                # Wait until the shell HWND is stable (past Widget.init splash).
                self._hwnd, self._title = wait_stable_shell_hwnd(
                    pid,
                    title_substr=self.title_substr,
                    timeout_sec=self.find_timeout_sec,
                    min_area=80_000,
                    stable_ms=900,
                )
            else:
                self._hwnd, self._title = find_window_by_title_substr(
                    self.title_substr, timeout_sec=self.find_timeout_sec
                )
        if not self._hwnd:
            self._mode = "skipped"
            self._error = "hwnd_timeout"
            return self.status()

        # Wait for map client / FlyCube Present before PrintWindow. BitBlt
        # during init_shell (HWND exists, viewports not attached) re-enters
        # DWM paint and has heap-corrupted the interact showcase.
        map_deadline = time.time() + min(20.0, max(4.0, self.find_timeout_sec))
        while time.time() < map_deadline:
            if not user32.IsWindow(int(self._hwnd)):
                self._mode = "skipped"
                self._error = "hwnd_died"
                return self.status()
            mapped, mapped_title = resolve_map_record_hwnd(int(self._hwnd))
            if mapped and is_usable_map_record_hwnd(mapped):
                if mapped_title:
                    self._title = mapped_title
                break
            time.sleep(0.2)
        time.sleep(0.25)

        # Retarget shell → map client / FlyCube Present so BitBlt is not a
        # WS_CLIPCHILDREN black hole (browse / Views ContentMapView).
        map_hwnd, map_title = resolve_map_record_hwnd(int(self._hwnd))
        if map_hwnd and map_hwnd != int(self._hwnd):
            self._hwnd = int(map_hwnd)
            if map_title:
                self._title = map_title

        self._rect = window_rect(self._hwnd)
        mode_pref = record_mode_pref(self._env)
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
            interval = 1.0 / self.fps
            while not self._burst_stop:
                t0 = time.time()
                if not user32.IsWindow(self._hwnd):
                    break
                # Re-query geometry every frame. A start-of-session lock of
                # 1280x800 misses window(resize) in ui.interact (~15s in).
                shell = int(self._hwnd)
                parent = int(user32.GetAncestor(shell, 2) or 0)  # GA_ROOT
                root = parent if parent else shell
                if not user32.IsWindow(root):
                    break
                mapped, mapped_title = resolve_map_record_hwnd(root)
                cap = int(root)
                if mapped and is_usable_map_record_hwnd(mapped):
                    # Map child that still fills the shell; otherwise the
                    # Views chrome resized and the child lagged.
                    if window_area(mapped) >= max(1, int(window_area(root) * 0.45)):
                        cap = int(mapped)
                        if mapped_title:
                            self._title = mapped_title
                    else:
                        cap = int(root)
                self._hwnd = cap
                self._rect = window_rect(cap)
                if not user32.IsWindowVisible(cap):
                    # Resize/restore can flicker WS_VISIBLE — skip, do not stop.
                    time.sleep(interval)
                    continue
                path = frames / f"frame_{i:05d}.bmp"
                if i == 0 or (i % 8) == 0:
                    bring_hwnd_to_front(root, stay_topmost=True)
                left, top, right, bottom = self._rect
                on_primary = rect_fully_on_primary(left, top, right, bottom)
                prefer_pw = not on_primary
                ok, near_black = capture_hwnd_bmp_ex(
                    cap,
                    path,
                    prefer_printwindow=prefer_pw,
                )
                if (not ok or near_black > 0.90) and prefer_pw:
                    ok2, nb2 = capture_hwnd_bmp_ex(
                        cap, path, prefer_printwindow=False
                    )
                    if ok2 and nb2 < near_black:
                        ok, near_black = ok2, nb2
                elif (not ok or near_black > 0.90) and not prefer_pw:
                    ok2, nb2 = capture_hwnd_bmp_ex(
                        cap, path, prefer_printwindow=True
                    )
                    if ok2 and nb2 < near_black:
                        ok, near_black = ok2, nb2
                if not ok or near_black > 0.98:
                    # Failed / flip-model black: skip this tick. Do not treat
                    # as teardown — that capped ui.interact at ~120 frames / 15s
                    # while the process still ran timeout_sec (180s).
                    try:
                        path.unlink(missing_ok=True)
                        path.with_suffix(path.suffix + ".method.txt").unlink(
                            missing_ok=True
                        )
                    except OSError:
                        pass
                    time.sleep(interval)
                    continue
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
            # Drop TOPMOST used during BitBlt burst (shell root + target).
            if self._hwnd:
                root = int(user32.GetAncestor(int(self._hwnd), 2) or 0)
                if root:
                    bring_hwnd_to_front(root, stay_topmost=False)
                bring_hwnd_to_front(int(self._hwnd), stay_topmost=False)
            # Best-effort: stitch frames → mp4 when ffmpeg is on PATH.
            # Keep frames dir as record_path for motion_gate unique-frame scoring.
            self._mp4_path = encode_frames_mp4(self._frames_dir, fps=self.fps)
        return self.status()

    def status(self) -> dict[str, Any]:
        out: dict[str, Any] = {
            "mode": self._mode,
            "record_mode_pref": record_mode_pref(self._env),
            "title_substr": self.title_substr,
            "hwnd": self._hwnd,
            "title": self._title,
            "virtual_screen": virtual_screen(),
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
                "on_primary": rect_fully_on_primary(left, top, right, bottom),
            }
        if self._path is not None:
            out["record_path"] = str(self._path)
            if self._path.is_file():
                out["bytes"] = self._path.stat().st_size
            elif self._path.is_dir():
                out["frame_count"] = sum(1 for _ in self._path.glob("frame_*.bmp"))
        mp4 = getattr(self, "_mp4_path", None)
        if mp4 is not None and Path(mp4).is_file():
            out["mp4_path"] = str(mp4)
            out["mp4_bytes"] = Path(mp4).stat().st_size
        if self._started:
            out["duration_s"] = round(time.time() - self._started, 3)
        if self._error:
            out["error"] = self._error
        if self._ffmpeg_skip:
            out["ffmpeg_skip"] = self._ffmpeg_skip
        return out


def json_dumps(obj: Any) -> str:
    import json

    return json.dumps(obj, indent=2)


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


if __name__ == "__main__":
    sys.exit(main())
