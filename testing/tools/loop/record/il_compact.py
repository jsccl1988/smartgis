# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Compact raw OS/agent events into Interact DSL (.il) statements."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

# Gaps larger than this become pump(ms=...).
DEFAULT_PUMP_GAP_MS = 80
# Max points kept in a path before emitting.
MAX_PATH_POINTS = 48
# Collapse near-duplicate path samples within this client-pixel distance.
PATH_MIN_DIST = 4
# pan_burst: similar short strokes.
PAN_MAX_LEN = 90
PAN_MIN_COUNT = 3
# wheel_burst clustering.
WHEEL_GAP_MS = 120
WHEEL_XY_TOL = 24


def _dist(a: tuple[int, int], b: tuple[int, int]) -> float:
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5


def load_events(path: Path) -> list[dict[str, Any]]:
    events: list[dict[str, Any]] = []
    text = path.read_text(encoding="utf-8")
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        obj = json.loads(line)
        if isinstance(obj, dict):
            events.append(obj)
    return events


def merge_by_time(
    os_events: list[dict[str, Any]],
    agent_events: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    merged = list(os_events) + list(agent_events)
    merged.sort(key=lambda e: int(e.get("t_ms", 0)))
    return merged


def compact_events(
    events: list[dict[str, Any]],
    *,
    pump_gap_ms: int = DEFAULT_PUMP_GAP_MS,
) -> list[dict[str, Any]]:
    """Return intermediate op dicts (op/driver/args) ready for IL emit."""
    ops: list[dict[str, Any]] = []
    i = 0
    n = len(events)
    last_t = 0

    def maybe_pump(t: int) -> None:
        nonlocal last_t
        gap = t - last_t
        if last_t > 0 and gap >= pump_gap_ms:
            ops.append({"op": "pump", "ms": int(gap), "driver": None})
        last_t = t

    while i < n:
        ev = events[i]
        t = int(ev.get("t_ms", 0))
        src = str(ev.get("src", "os"))

        if src == "agent":
            maybe_pump(t)
            kind = str(ev.get("kind", ""))
            if kind == "select_map_tab":
                ops.append(
                    {
                        "op": "select_map_tab",
                        "index": int(ev.get("index", 0)),
                        "driver": "inproc",
                    }
                )
            elif kind == "mark":
                ops.append(
                    {
                        "op": "mark",
                        "token": str(ev.get("token", "")),
                        "driver": "inproc",
                    }
                )
            elif kind == "run_tool":
                ops.append(
                    {
                        "op": "run_command",
                        "id": str(ev.get("id", "")),
                        "driver": "inproc",
                    }
                )
            elif kind == "window":
                ops.append(
                    {
                        "op": "window",
                        "action": str(ev.get("action", "activate")),
                        "driver": "inproc",
                    }
                )
            i += 1
            continue

        kind = str(ev.get("kind", ""))
        if kind == "key" and ev.get("action") == "down":
            maybe_pump(t)
            name = str(ev.get("name") or f"VK_{ev.get('vk', 0)}")
            # Skip pure modifier downs without a following non-mod key in chord — emit key only.
            if name in ("SHIFT", "CTRL", "ALT"):
                i += 1
                continue
            ops.append({"op": "key", "name": name, "driver": "os"})
            # Skip matching up if present.
            if i + 1 < n:
                nxt = events[i + 1]
                if (
                    nxt.get("kind") == "key"
                    and nxt.get("action") == "up"
                    and nxt.get("vk") == ev.get("vk")
                ):
                    i += 1
            i += 1
            continue

        if kind == "mouse" and ev.get("action") == "wheel":
            maybe_pump(t)
            wheels = [ev]
            j = i + 1
            while j < n:
                w = events[j]
                if w.get("src") == "agent":
                    break
                if w.get("kind") != "mouse" or w.get("action") != "wheel":
                    break
                if int(w.get("t_ms", 0)) - int(wheels[-1].get("t_ms", 0)) > WHEEL_GAP_MS:
                    break
                if abs(int(w.get("x", 0)) - int(ev.get("x", 0))) > WHEEL_XY_TOL:
                    break
                if abs(int(w.get("y", 0)) - int(ev.get("y", 0))) > WHEEL_XY_TOL:
                    break
                wheels.append(w)
                j += 1
            x = int(ev.get("x", 0))
            y = int(ev.get("y", 0))
            delta = int(ev.get("delta", -120))
            if len(wheels) >= 2:
                gaps = [
                    max(
                        0,
                        int(wheels[k].get("t_ms", 0))
                        - int(wheels[k - 1].get("t_ms", 0)),
                    )
                    for k in range(1, len(wheels))
                ]
                pump_ms = int(sum(gaps) / len(gaps)) if gaps else 50
                ops.append(
                    {
                        "op": "wheel_burst",
                        "count": len(wheels),
                        "x": x,
                        "y": y,
                        "delta": delta,
                        "pump_ms": max(20, pump_ms),
                        "driver": "os",
                    }
                )
            else:
                ops.append(
                    {
                        "op": "wheel",
                        "x": x,
                        "y": y,
                        "delta": delta,
                        "driver": "os",
                    }
                )
            i = j
            continue

        if kind == "mouse" and ev.get("action") == "down" and ev.get("button") == "left":
            maybe_pump(t)
            points: list[tuple[int, int]] = [
                (int(ev.get("x", 0)), int(ev.get("y", 0)))
            ]
            j = i + 1
            up_pt: tuple[int, int] | None = None
            while j < n:
                m = events[j]
                if m.get("src") == "agent":
                    break
                if m.get("kind") != "mouse":
                    break
                act = m.get("action")
                if act == "move" and m.get("button") == "left":
                    pt = (int(m.get("x", 0)), int(m.get("y", 0)))
                    if _dist(pt, points[-1]) >= PATH_MIN_DIST:
                        points.append(pt)
                    j += 1
                    continue
                if act == "up" and m.get("button") == "left":
                    up_pt = (int(m.get("x", 0)), int(m.get("y", 0)))
                    if _dist(up_pt, points[-1]) >= PATH_MIN_DIST:
                        points.append(up_pt)
                    j += 1
                    break
                break
            if up_pt is None and len(points) == 1:
                # Click without up in stream — treat as click.
                ops.append(
                    {
                        "op": "click",
                        "target": "shell",
                        "x": points[0][0],
                        "y": points[0][1],
                        "driver": "os",
                    }
                )
                i = j if j > i else i + 1
                continue

            stroke_len = _dist(points[0], points[-1])
            # Short strokes (incl. 2-point drag): try pan_burst coalescing.
            if stroke_len <= PAN_MAX_LEN and len(points) <= 6:
                strokes = [
                    {
                        "x0": points[0][0],
                        "y0": points[0][1],
                        "x1": points[-1][0],
                        "y1": points[-1][1],
                        "t": t,
                        "end_j": j,
                    }
                ]
                k = j
                while k < n and len(strokes) < 20:
                    e2 = events[k]
                    if e2.get("src") == "agent":
                        break
                    if not (
                        e2.get("kind") == "mouse"
                        and e2.get("action") == "down"
                        and e2.get("button") == "left"
                    ):
                        break
                    pts2: list[tuple[int, int]] = [
                        (int(e2.get("x", 0)), int(e2.get("y", 0)))
                    ]
                    m = k + 1
                    while m < n:
                        mm = events[m]
                        if mm.get("kind") != "mouse":
                            break
                        if mm.get("action") == "move" and mm.get("button") == "left":
                            pt = (int(mm.get("x", 0)), int(mm.get("y", 0)))
                            if _dist(pt, pts2[-1]) >= PATH_MIN_DIST:
                                pts2.append(pt)
                            m += 1
                            continue
                        if mm.get("action") == "up" and mm.get("button") == "left":
                            pts2.append((int(mm.get("x", 0)), int(mm.get("y", 0))))
                            m += 1
                            break
                        break
                    if len(pts2) < 2:
                        break
                    if _dist(pts2[0], pts2[-1]) > PAN_MAX_LEN:
                        break
                    strokes.append(
                        {
                            "x0": pts2[0][0],
                            "y0": pts2[0][1],
                            "x1": pts2[-1][0],
                            "y1": pts2[-1][1],
                            "t": int(e2.get("t_ms", 0)),
                            "end_j": m,
                        }
                    )
                    k = m
                if len(strokes) >= PAN_MIN_COUNT:
                    dx = int(
                        sum(s["x1"] - s["x0"] for s in strokes) / len(strokes)
                    )
                    dy = int(
                        sum(s["y1"] - s["y0"] for s in strokes) / len(strokes)
                    )
                    gaps = [
                        max(0, strokes[g]["t"] - strokes[g - 1]["t"])
                        for g in range(1, len(strokes))
                    ]
                    pump_ms = int(sum(gaps) / len(gaps)) if gaps else 45
                    ops.append(
                        {
                            "op": "pan_burst",
                            "count": len(strokes),
                            "x": strokes[0]["x0"],
                            "y": strokes[0]["y0"],
                            "dx": dx,
                            "dy": dy,
                            "pump_ms": max(20, pump_ms),
                            "driver": "os",
                        }
                    )
                    i = strokes[-1]["end_j"]
                    continue

            if len(points) <= 2:
                x0, y0 = points[0]
                x1, y1 = points[-1]
                if _dist(points[0], points[-1]) < PATH_MIN_DIST:
                    ops.append(
                        {
                            "op": "click",
                            "target": "shell",
                            "x": x0,
                            "y": y0,
                            "driver": "os",
                        }
                    )
                else:
                    ops.append(
                        {
                            "op": "drag",
                            "x0": x0,
                            "y0": y0,
                            "x1": x1,
                            "y1": y1,
                            "driver": "os",
                        }
                    )
                i = j
                continue

            if len(points) > MAX_PATH_POINTS:
                step = max(1, len(points) // MAX_PATH_POINTS)
                slim = points[::step]
                if slim[-1] != points[-1]:
                    slim.append(points[-1])
                points = slim
            ops.append({"op": "path", "points": points, "driver": "os"})
            i = j
            continue

        if kind == "mouse" and ev.get("action") == "down" and ev.get("button") == "right":
            maybe_pump(t)
            x, y = int(ev.get("x", 0)), int(ev.get("y", 0))
            ops.append(
                {"op": "rclick", "target": "shell", "x": x, "y": y, "driver": "os"}
            )
            if i + 1 < n:
                nxt = events[i + 1]
                if (
                    nxt.get("kind") == "mouse"
                    and nxt.get("action") == "up"
                    and nxt.get("button") == "right"
                ):
                    i += 1
            i += 1
            continue

        i += 1

    return ops


def _fmt_points(points: list[tuple[int, int]]) -> str:
    return ", ".join(f"[{p[0]}, {p[1]}]" for p in points)


def emit_il(
    ops: list[dict[str, Any]],
    *,
    script_name: str = "recorded",
    title_comment: str = "",
) -> str:
    lines: list[str] = [
        "// Copyright (c) 2026 The Mogu Authors.",
        "// All rights reserved.",
    ]
    if title_comment:
        lines.append(f"// {title_comment}")
    else:
        lines.append("// Auto-generated by loop/record/il_recorder (hybrid OS+agent).")
    lines.append("")
    lines.append(f'script "{script_name}" {{')
    lines.append("  window(activate);")
    lines.append("  pump(ms=200);")

    # Group consecutive @os ops into one seq @os block; emit @inproc standalone.
    os_buf: list[str] = []

    def flush_os() -> None:
        nonlocal os_buf
        if not os_buf:
            return
        lines.append("  seq @os {")
        for stmt in os_buf:
            lines.append(f"    {stmt}")
        lines.append("  }")
        os_buf = []

    for op in ops:
        driver = op.get("driver")
        name = op.get("op")
        if name == "pump":
            stmt = f"pump(ms={int(op.get('ms', 100))});"
            if driver == "inproc":
                flush_os()
                lines.append(f"  {stmt}")
            else:
                os_buf.append(stmt)
            continue
        if driver == "inproc":
            flush_os()
            if name == "select_map_tab":
                lines.append(
                    f"  select_map_tab({int(op.get('index', 0))}) @inproc;"
                )
            elif name == "mark":
                tok = str(op.get("token", "")).replace('"', '\\"')
                lines.append(f'  mark("{tok}") @inproc;')
            elif name == "run_command":
                cid = str(op.get("id", "")).replace('"', '\\"')
                lines.append(f'  run_command("{cid}") @inproc;')
            elif name == "window":
                action = str(op.get("action", "activate"))
                lines.append(f"  window({action}) @inproc;")
            else:
                lines.append(f"  // skipped unknown inproc op: {name}")
            continue

        # OS verbs
        if name == "click":
            os_buf.append(
                f"click(shell, {int(op['x'])}, {int(op['y'])});"
            )
        elif name == "rclick":
            os_buf.append(
                f"rclick(shell, {int(op['x'])}, {int(op['y'])});"
            )
        elif name == "drag":
            os_buf.append(
                "drag("
                f"x0={int(op['x0'])}, y0={int(op['y0'])}, "
                f"x1={int(op['x1'])}, y1={int(op['y1'])});"
            )
        elif name == "path":
            pts = op.get("points") or []
            os_buf.append(f"path({_fmt_points(pts)});")
        elif name == "pan_burst":
            os_buf.append(
                "pan_burst("
                f"count={int(op['count'])}, "
                f"x={int(op['x'])}, y={int(op['y'])}, "
                f"dx={int(op['dx'])}, dy={int(op['dy'])}, "
                f"pump_ms={int(op['pump_ms'])});"
            )
        elif name == "wheel_burst":
            os_buf.append(
                "wheel_burst("
                f"count={int(op['count'])}, "
                f"x={int(op['x'])}, y={int(op['y'])}, "
                f"delta={int(op['delta'])}, "
                f"pump_ms={int(op['pump_ms'])});"
            )
        elif name == "wheel":
            os_buf.append(
                f"wheel(x={int(op['x'])}, y={int(op['y'])}, "
                f"delta={int(op['delta'])});"
            )
        elif name == "key":
            os_buf.append(f"key({op.get('name', 'ESCAPE')});")
        else:
            os_buf.append(f"// skipped {name}")

    flush_os()
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def events_to_il(
    events: list[dict[str, Any]],
    *,
    script_name: str = "recorded",
    title_comment: str = "",
    pump_gap_ms: int = DEFAULT_PUMP_GAP_MS,
) -> str:
    ops = compact_events(events, pump_gap_ms=pump_gap_ms)
    return emit_il(ops, script_name=script_name, title_comment=title_comment)
