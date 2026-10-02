# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""OS-level interact script runner: PostMessage and/or SendInput against HWND."""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any

from .input_api import (  # noqa: F401 — re-export for plain_browse / callers
    MOUSEEVENTF_WHEEL,
    WM_KEYDOWN,
    WM_KEYUP,
    _client_to_screen,
    _mod_vk,
    _send_key,
    _send_mouse_abs,
    find_window_by_title,
    user32,
)
from .steps import _run_step  # noqa: F401

def _capture_zoom_sidecar(
    hwnd: int,
    path: Path,
) -> dict[str, Any]:
    """HWND BMP for zoom_before/after gate; writes .method.txt sidecar."""
    from ..record.hwnd import capture_hwnd_resilient

    return capture_hwnd_resilient(
        int(hwnd), Path(path), clear_existing=True, settle_sec=0.15, retry_sleep_sec=0.2
    )


def run_script(
    script_path: Path,
    hwnd: int,
    *,
    inject_default: str = "postmessage",
    captures_root: Path | None = None,
    zoom_gate: dict[str, Any] | None = None,
    click_gate: dict[str, Any] | None = None,
    capture_hwnd: int | None = None,
) -> dict[str, Any]:
    report: dict[str, Any] = {
        "ok": True,
        "script": str(script_path),
        "hwnd": hwnd,
        "steps_run": 0,
        "steps_skipped": 0,
        "errors": [],
        "clicks": 0,
        "dblclicks": 0,
    }
    if not hwnd or not user32.IsWindow(hwnd):
        report["ok"] = False
        report["errors"].append("invalid_hwnd")
        return report

    zoom_cfg = zoom_gate if isinstance(zoom_gate, dict) else None
    click_cfg = click_gate if isinstance(click_gate, dict) else None
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
        op = step.get("op")
        _run_step(
            step,
            hwnd,
            inject_default=inject_default,
            report=report,
            shell_hwnd=shell_hwnd,
        )
        if op == "click":
            report["clicks"] = int(report.get("clicks") or 0) + 1
            # Capture early (window still healthy) — end-of-script BitBlt can
            # go black after long SendInput + record TOPMOST races.
            if (
                click_cfg is not None
                and captures_root is not None
                and bool(click_cfg.get("enabled", True))
                and int(report.get("clicks") or 0) == 1
            ):
                after_rel = str(
                    click_cfg.get("after") or "legacy/_click_after.bmp"
                )
                after_path = Path(captures_root) / after_rel.replace("\\", "/")
                time.sleep(0.35)
                report["click_capture"] = {
                    "after": _capture_zoom_sidecar(shell_hwnd, after_path),
                    "clicks": int(report.get("clicks") or 0),
                    "dblclicks": int(report.get("dblclicks") or 0),
                    "when": "first_click",
                }
        elif op == "dblclick":
            report["dblclicks"] = int(report.get("dblclicks") or 0) + 1
            if (
                click_cfg is not None
                and captures_root is not None
                and bool(click_cfg.get("enabled", True))
            ):
                after_rel = str(
                    click_cfg.get("after") or "legacy/_click_after.bmp"
                )
                after_path = Path(captures_root) / after_rel.replace("\\", "/")
                prev = report.get("click_capture") or {}
                prev_after = (
                    prev.get("after") if isinstance(prev, dict) else None
                )
                prev_ok = (
                    isinstance(prev_after, dict)
                    and bool(prev_after.get("ok"))
                    and float(prev_after.get("near_black") or 1.0) < 0.55
                )
                time.sleep(0.35)
                after_cap = _capture_zoom_sidecar(shell_hwnd, after_path)
                # Keep the earlier good frame if end-of-script capture went black.
                if (
                    (not after_cap.get("ok")
                     or float(after_cap.get("near_black") or 1.0) > 0.55)
                    and prev_ok
                    and isinstance(prev_after, dict)
                ):
                    try:
                        src = Path(str(prev_after.get("path") or ""))
                        if src.is_file() and src.stat().st_size > 1000:
                            after_path.write_bytes(src.read_bytes())
                            after_cap = dict(prev_after)
                            after_cap["fallback"] = "first_click"
                    except OSError:
                        pass
                report["click_capture"] = {
                    "after": after_cap,
                    "clicks": int(report.get("clicks") or 0),
                    "dblclicks": int(report.get("dblclicks") or 0),
                    "when": "dblclick",
                }

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
                if op == "wm_command":
                    # MDI switch (e.g. ID_WND_3D) — retarget inject to the new view.
                    time.sleep(0.8)
                    try:
                        from ..record.hwnd import find_map_client_hwnd

                        new_hwnd, how = find_map_client_hwnd(
                            shell_hwnd, timeout_sec=10.0
                        )
                        if new_hwnd and user32.IsWindow(int(new_hwnd)):
                            hwnd = int(new_hwnd)
                            report["retarget_hwnd"] = hwnd
                            report["retarget_how"] = how
                    except Exception as retarget_exc:  # noqa: BLE001
                        report.setdefault("errors", []).append(
                            f"retarget: {retarget_exc}"
                        )
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
