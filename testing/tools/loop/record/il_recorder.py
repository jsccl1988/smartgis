# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Open/attach app, record input, write events.jsonl + replayable .il."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, TextIO

_TOOLS = Path(__file__).resolve().parents[2]
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from loop.record.agent_events import (  # noqa: E402
    disable_agent_record,
    enable_agent_record,
    poll_agent_events,
    try_connect_agent,
)
from loop.record.hwnd import find_window_by_title_substr  # noqa: E402
from loop.record.il_compact import events_to_il, merge_by_time  # noqa: E402
from loop.record.os_hook import OsInputHook  # noqa: E402


def _stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def _default_captures(config: str) -> Path:
    repo = Path(__file__).resolve().parents[4]
    return repo / "out" / config / "captures" / "record"


def _resolve_exe(exe: str | None, config: str) -> Path | None:
    if not exe:
        repo = Path(__file__).resolve().parents[4]
        cand = repo / "out" / config / "SmartGisViews.exe"
        return cand if cand.is_file() else None
    p = Path(exe)
    if p.is_file():
        return p
    repo = Path(__file__).resolve().parents[4]
    cand = repo / "out" / config / exe
    if cand.is_file():
        return cand
    return p if p.exists() else None


class EventWriter:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._fp: TextIO = self.path.open("w", encoding="utf-8", newline="\n")
        self._lock = threading.Lock()
        self.events: list[dict[str, Any]] = []

    def write(self, ev: dict[str, Any]) -> None:
        line = json.dumps(ev, ensure_ascii=False, separators=(",", ":"))
        with self._lock:
            self._fp.write(line + "\n")
            self._fp.flush()
            self.events.append(ev)

    def close(self) -> None:
        with self._lock:
            self._fp.close()


def run_record_session(
    *,
    title: str = "SmartGIS Views",
    exe: Path | None = None,
    attach: bool = False,
    out_dir: Path,
    script_name: str = "recorded",
    hwnd_timeout_sec: float = 60.0,
    also_video: bool = False,
    hotkey: bool = True,
    agent: bool = True,
    launch_args: list[str] | None = None,
) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = _stamp()
    session_dir = out_dir / f"il_{stamp}"
    session_dir.mkdir(parents=True, exist_ok=True)

    proc: subprocess.Popen[str] | None = None
    report: dict[str, Any] = {
        "ok": False,
        "session_dir": str(session_dir),
        "title": title,
        "attach": attach,
    }

    if not attach:
        if exe is None or not exe.is_file():
            report["error"] = f"exe not found: {exe}"
            return report
        env = os.environ.copy()
        env.setdefault("SG_DEBUG", "1")
        cmd = [str(exe)] + list(launch_args or [])
        print(f"LAUNCH: {' '.join(cmd)}", flush=True)
        proc = subprocess.Popen(
            cmd,
            cwd=str(exe.parent),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        report["pid"] = proc.pid
        report["exe"] = str(exe)

    hwnd, win_title = find_window_by_title_substr(title, timeout_sec=hwnd_timeout_sec)
    if not hwnd:
        report["error"] = f"window not found for title substr={title!r}"
        if proc is not None:
            proc.terminate()
        return report
    report["hwnd"] = hwnd
    report["window_title"] = win_title
    print(f"HWND: {hwnd} title={win_title!r}", flush=True)
    print(
        "Recording… interact with the app. Stop: Ctrl+Shift+F9 or Enter / Ctrl+C.",
        flush=True,
    )

    events_path = session_dir / "events.jsonl"
    writer = EventWriter(events_path)
    t0 = time.perf_counter()

    agent_client = None
    if agent:
        # Wait a bit for discovery after launch.
        for _ in range(30):
            agent_client = try_connect_agent(timeout=0.5)
            if agent_client is not None:
                break
            time.sleep(0.2)
        if agent_client is not None:
            enabled = enable_agent_record(agent_client)
            report["agent"] = {"connected": True, "record_enable": enabled}
            print(f"DebugAgent: connected (record.enable={enabled})", flush=True)
        else:
            report["agent"] = {"connected": False}
            print("DebugAgent: not available — OS-only .il", flush=True)

    video_rec = None
    if also_video:
        try:
            from loop.record.hwnd import HwndRecorder

            video_rec = HwndRecorder(
                captures_dir=out_dir,
                suite_id=f"il_{stamp}",
                title_substr=title,
                fps=10.0,
            )
            video_rec.bind_hwnd(int(hwnd), win_title)
            report["video_start"] = video_rec.start_after_hwnd(wait_for_hwnd=False)
        except Exception as exc:  # noqa: BLE001
            report["video_error"] = str(exc)

    hook = OsInputHook(
        target_hwnd=int(hwnd),
        on_event=writer.write,
        t0=t0,
        enable_hotkey=hotkey,
    )
    hook.start()

    stop_reason = "unknown"
    agent_poll_stop = threading.Event()

    def _agent_poll_loop() -> None:
        while not agent_poll_stop.wait(0.15):
            for ev in poll_agent_events(agent_client, t0=t0):
                writer.write(ev)

    poll_thread = None
    if agent_client is not None:
        poll_thread = threading.Thread(
            target=_agent_poll_loop, name="agent-record-poll", daemon=True
        )
        poll_thread.start()

    def _stdin_wait() -> None:
        nonlocal stop_reason
        try:
            sys.stdin.readline()
            stop_reason = "enter"
            hook.stop()
        except Exception:
            pass

    stdin_thread = threading.Thread(target=_stdin_wait, name="record-stdin", daemon=True)
    stdin_thread.start()

    try:
        if hook.wait_stop(timeout=None):
            stop_reason = "hotkey"
        elif stop_reason == "unknown":
            stop_reason = "stop"
    except KeyboardInterrupt:
        stop_reason = "ctrl_c"
        hook.stop()
    finally:
        agent_poll_stop.set()
        if poll_thread:
            poll_thread.join(timeout=2.0)
        # Final agent drain.
        for ev in poll_agent_events(agent_client, t0=t0):
            writer.write(ev)
        disable_agent_record(agent_client)
        if agent_client is not None:
            try:
                agent_client.close()
            except Exception:
                pass
        writer.close()
        if video_rec is not None:
            report["video"] = video_rec.stop()

    # Compact
    os_events = [e for e in writer.events if e.get("src", "os") == "os"]
    agent_events = [e for e in writer.events if e.get("src") == "agent"]
    merged = merge_by_time(os_events, agent_events)
    il_text = events_to_il(
        merged,
        script_name=script_name,
        title_comment=f"Recorded from HWND title={win_title!r} reason={stop_reason}",
    )
    il_path = session_dir / f"{script_name}.il"
    il_path.write_text(il_text, encoding="utf-8", newline="\n")

    report["ok"] = True
    report["stop_reason"] = stop_reason
    report["events_path"] = str(events_path)
    report["il_path"] = str(il_path)
    report["event_count"] = len(writer.events)
    report["os_events"] = len(os_events)
    report["agent_events"] = len(agent_events)
    report["duration_s"] = round(time.perf_counter() - t0, 3)

    # Leave process running unless we launched and user wants kill — keep app open.
    report["proc_left_running"] = proc is not None and proc.poll() is None
    print(json.dumps(report, indent=2), flush=True)
    print(f"Wrote {il_path}", flush=True)
    return report


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        description="Record app interactions to Interact DSL (.il) for repro"
    )
    p.add_argument(
        "--title",
        default="SmartGIS Views",
        help="Window title substring (default: SmartGIS Views)",
    )
    p.add_argument(
        "--exe",
        default=None,
        help="Exe to launch (default: out/<config>/SmartGisViews.exe)",
    )
    p.add_argument(
        "--attach",
        action="store_true",
        help="Do not launch; attach to existing window by --title",
    )
    p.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Output captures/record dir (default: out/<config>/captures/record)",
    )
    p.add_argument(
        "--config",
        choices=("Debug", "Release"),
        default="Debug",
        help="out/<config> when resolving default exe/out",
    )
    p.add_argument("--name", default="recorded", help="script name / .il stem")
    p.add_argument(
        "--video",
        action="store_true",
        help="Also record HWND video/BMP via HwndRecorder",
    )
    p.add_argument(
        "--no-hotkey",
        action="store_true",
        help="Disable Ctrl+Shift+F9 (console Enter / Ctrl+C only)",
    )
    p.add_argument(
        "--no-agent",
        action="store_true",
        help="Skip DebugAgent semantic poll",
    )
    p.add_argument(
        "launch_args",
        nargs="*",
        help="Extra args forwarded to exe when launching",
    )
    args = p.parse_args(argv)

    out_dir = args.out or _default_captures(args.config)
    exe_path = None if args.attach else _resolve_exe(args.exe, args.config)
    if not args.attach and (exe_path is None or not exe_path.is_file()):
        print(f"error: exe not found ({args.exe!r} / out/{args.config})", flush=True)
        return 2

    report = run_record_session(
        title=args.title,
        exe=exe_path,
        attach=args.attach,
        out_dir=out_dir,
        script_name=args.name,
        also_video=args.video,
        hotkey=not args.no_hotkey,
        agent=not args.no_agent,
        launch_args=list(args.launch_args),
    )
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
