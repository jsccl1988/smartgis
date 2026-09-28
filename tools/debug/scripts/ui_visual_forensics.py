# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Mode C UI visual forensics: offline analyze and live DebugAgent capture.

See also ``ui_smoke.py`` for a minimal Agent connectivity check.
"""

from __future__ import annotations

import argparse
import base64
import binascii
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

_PACKAGE_ROOT = Path(__file__).resolve().parents[1]
_PYTHON_ROOT = _PACKAGE_ROOT.parent
for path in (str(_PYTHON_ROOT), str(_PACKAGE_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from smartgis.client import AgentClient, AgentError  # noqa: E402

_FRAME_GLOB = "frame_*.png"
_COLLAPSED_Y_THRESHOLD = 8
_COLLAPSED_X_THRESHOLD = 12
_GANTT_MIN_LANE_GAP = 12


def _discovery_path() -> Path:
    temp = os.environ.get("TEMP") or os.environ.get("TMP") or "."
    return Path(temp) / "smartgis-debug.json"


def _load_agent_url() -> str:
    path = _discovery_path()
    if not path.is_file():
        raise SystemExit(
            f"discovery file missing: {path} (start SmartGIS with debug console)"
        )
    data = json.loads(path.read_text(encoding="utf-8"))
    host = str(data.get("host", "127.0.0.1"))
    port = int(data["port"])
    return f"tcp://{host}:{port}"


def _forensics_root() -> Path:
    return Path("out") / "ui_forensics"


def _is_unavailable(text: str) -> bool:
    lowered = text.strip().lower()
    if lowered in ("unavailable", "not wired"):
        return True
    return "not bound" in lowered or lowered.startswith("error:")


def _view_bounds(view: dict[str, Any]) -> tuple[int, int, int, int] | None:
    try:
        x = int(view["x"])
        y = int(view["y"])
        w = int(view["w"])
        h = int(view["h"])
    except (KeyError, TypeError, ValueError):
        return None
    if w <= 0 or h <= 0:
        return None
    return x, y, x + w, y + h


def _overlap_area(
    a: tuple[int, int, int, int], b: tuple[int, int, int, int]
) -> int:
    ix0 = max(a[0], b[0])
    iy0 = max(a[1], b[1])
    ix1 = min(a[2], b[2])
    iy1 = min(a[3], b[3])
    if ix1 <= ix0 or iy1 <= iy0:
        return 0
    return (ix1 - ix0) * (iy1 - iy0)


def _view_role(view: dict[str, Any]) -> str:
    role = view.get("role")
    return str(role) if role is not None else ""


def _view_parent(view: dict[str, Any]) -> str | None:
    if "parent_id" not in view:
        return None
    parent = view.get("parent_id")
    if parent is None:
        return None
    return str(parent)


def _detect_sibling_overlaps(
    views: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    by_parent: dict[str | None, list[tuple[int, dict[str, Any]]]] = {}
    for index, view in enumerate(views):
        bounds = _view_bounds(view)
        if bounds is None:
            continue
        parent = _view_parent(view)
        by_parent.setdefault(parent, []).append((index, view))

    for parent_id, group in by_parent.items():
        for i in range(len(group)):
            idx_a, view_a = group[i]
            bounds_a = _view_bounds(view_a)
            assert bounds_a is not None
            for j in range(i + 1, len(group)):
                idx_b, view_b = group[j]
                bounds_b = _view_bounds(view_b)
                assert bounds_b is not None
                area = _overlap_area(bounds_a, bounds_b)
                if area <= 0:
                    continue
                issues.append(
                    {
                        "kind": "sibling-aabb-overlap",
                        "parent_id": parent_id,
                        "area": area,
                        "a": {"index": idx_a, "role": _view_role(view_a), "bounds": view_a},
                        "b": {"index": idx_b, "role": _view_role(view_b), "bounds": view_b},
                    }
                )
    return issues


def _detect_tabstrip_overlaps(
    views: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    tabs: list[tuple[int, dict[str, Any], tuple[int, int, int, int]]] = []
    for index, view in enumerate(views):
        if "tab" not in _view_role(view).lower():
            continue
        bounds = _view_bounds(view)
        if bounds is None:
            continue
        tabs.append((index, view, bounds))

    for i in range(len(tabs)):
        idx_a, view_a, bounds_a = tabs[i]
        for j in range(i + 1, len(tabs)):
            idx_b, view_b, bounds_b = tabs[j]
            if _overlap_area(bounds_a, bounds_b) <= 0:
                continue
            issues.append(
                {
                    "kind": "tabstrip-overlap",
                    "a": {"index": idx_a, "role": _view_role(view_a), "bounds": view_a},
                    "b": {"index": idx_b, "role": _view_role(view_b), "bounds": view_b},
                }
            )
    return issues


def _is_button_like(view: dict[str, Any]) -> bool:
    role = _view_role(view).lower()
    return "button" in role or role.endswith("-btn") or role.endswith("_btn")


def _detect_collapsed_y_buttons(
    views: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    by_parent: dict[str | None, list[tuple[int, dict[str, Any]]]] = {}
    for index, view in enumerate(views):
        if not _is_button_like(view):
            continue
        bounds = _view_bounds(view)
        if bounds is None:
            continue
        parent = _view_parent(view)
        by_parent.setdefault(parent, []).append((index, view))

    for parent_id, group in by_parent.items():
        for i in range(len(group)):
            idx_a, view_a = group[i]
            bounds_a = _view_bounds(view_a)
            assert bounds_a is not None
            ya = int(view_a["y"])
            xa = int(view_a["x"])
            for j in range(i + 1, len(group)):
                idx_b, view_b = group[j]
                yb = int(view_b["y"])
                xb = int(view_b["x"])
                if abs(ya - yb) >= _COLLAPSED_Y_THRESHOLD:
                    continue
                if abs(xa - xb) > _COLLAPSED_X_THRESHOLD:
                    continue
                issues.append(
                    {
                        "kind": "collapsed-y",
                        "parent_id": parent_id,
                        "dy": abs(ya - yb),
                        "a": {"index": idx_a, "role": _view_role(view_a), "bounds": view_a},
                        "b": {"index": idx_b, "role": _view_role(view_b), "bounds": view_b},
                    }
                )
    return issues


def _detect_gantt_lane_gaps(
    lanes: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    parsed: list[tuple[str, int]] = []
    for lane in lanes:
        if not isinstance(lane, dict):
            continue
        try:
            y = int(lane["y"])
        except (KeyError, TypeError, ValueError):
            continue
        name = str(lane.get("name", ""))
        parsed.append((name, y))
    parsed.sort(key=lambda item: item[1])
    for i in range(len(parsed) - 1):
        name_a, y_a = parsed[i]
        name_b, y_b = parsed[i + 1]
        delta = y_b - y_a
        if delta >= _GANTT_MIN_LANE_GAP:
            continue
        issues.append(
            {
                "kind": "gantt-lane-gap",
                "lane_a": name_a,
                "lane_b": name_b,
                "y_a": y_a,
                "y_b": y_b,
                "delta": delta,
                "min_required": _GANTT_MIN_LANE_GAP,
            }
        )
    return issues


def _layout_issue_lines(layout_text: str) -> list[str]:
    """Return real issue lines; ignore blanks and '#' comments (e.g. '# clean')."""
    issues: list[str] = []
    for raw in layout_text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        issues.append(line)
    return issues


def _analyze_run(run_dir: Path) -> dict[str, Any]:
    summary: dict[str, Any] = {
        "run_dir": str(run_dir),
        "clean": True,
        "frames": [],
        "violations": [],
        "notes": [],
    }

    manifest_path = run_dir / "manifest.json"
    manifest: dict[str, Any] = {}
    if manifest_path.is_file():
        try:
            loaded = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
            if isinstance(loaded, dict):
                manifest = loaded
                summary["manifest"] = {"path": str(manifest_path), "present": True}
            else:
                summary["notes"].append("manifest.json is not a JSON object")
        except json.JSONDecodeError as exc:
            summary["notes"].append(f"manifest.json parse error: {exc}")
    else:
        summary["notes"].append("manifest.json missing")

    frame_paths = sorted(run_dir.glob(_FRAME_GLOB))
    for path in frame_paths:
        summary["frames"].append({"name": path.name, "exists": path.is_file()})

    layout_path = run_dir / "layout_issues.txt"
    layout_text = ""
    if layout_path.is_file():
        layout_text = layout_path.read_text(encoding="utf-8")
    issue_lines = _layout_issue_lines(layout_text)
    if issue_lines:
        summary["layout_issues"] = {
            "path": str(layout_path),
            "non_empty": True,
            "lines": len(issue_lines),
        }
        summary["violations"].append(
            {
                "kind": "layout_issues.txt",
                "detail": f"{len(issue_lines)} layout issue line(s)",
            }
        )
    else:
        summary["layout_issues"] = {
            "path": str(layout_path),
            "non_empty": False,
            "lines": 0,
        }

    views = manifest.get("views")
    if isinstance(views, list):
        view_dicts = [v for v in views if isinstance(v, dict)]
        summary["views_analyzed"] = len(view_dicts)
        for issue in _detect_sibling_overlaps(view_dicts):
            summary["violations"].append(issue)
        for issue in _detect_tabstrip_overlaps(view_dicts):
            summary["violations"].append(issue)
        for issue in _detect_collapsed_y_buttons(view_dicts):
            summary["violations"].append(issue)
    elif "views" in manifest:
        summary["notes"].append("manifest.views is not an array")

    gantt_lanes = manifest.get("gantt_lanes")
    if isinstance(gantt_lanes, list):
        for issue in _detect_gantt_lane_gaps(gantt_lanes):
            summary["violations"].append(issue)
    elif "gantt_lanes" in manifest:
        summary["notes"].append("manifest.gantt_lanes is not an array")

    if summary["violations"]:
        summary["clean"] = False

    return summary


def _list_runs() -> list[str]:
    root = _forensics_root()
    if not root.is_dir():
        return []
    names: list[str] = []
    for child in root.iterdir():
        if child.is_dir():
            names.append(child.name)
    return sorted(names)


def _call_ui_raw(
    client: AgentClient, method: str, params: dict[str, Any] | None = None
) -> tuple[dict[str, Any], str | None]:
    """Return (result dict, text field if any). Raises AgentError on transport/RPC failure."""
    result = client.call(method, params or {})
    text = result.get("text")
    if isinstance(text, str):
        return result, text
    return result, None


def _call_ui_text(
    client: AgentClient, method: str, params: dict[str, Any] | None = None
) -> str:
    try:
        result, text = _call_ui_raw(client, method, params)
    except AgentError as exc:
        return f"error: {exc}"
    if text is not None:
        return text
    return json.dumps(result, ensure_ascii=False)


def _extract_png_bytes(result: dict[str, Any], text: str | None) -> bytes | None:
    for key in ("png", "png_b64", "png_base64", "image", "data", "bytes"):
        if key not in result:
            continue
        value = result[key]
        if isinstance(value, (bytes, bytearray)):
            return bytes(value)
        if isinstance(value, str):
            try:
                return base64.b64decode(value, validate=True)
            except (ValueError, binascii.Error):
                try:
                    return base64.b64decode(value)
                except (ValueError, binascii.Error):
                    pass

    if text is None:
        return None
    if _is_unavailable(text):
        return None
    stripped = text.strip()
    if stripped.startswith("{") or stripped.startswith("["):
        try:
            nested = json.loads(stripped)
        except json.JSONDecodeError:
            nested = None
        if isinstance(nested, dict):
            return _extract_png_bytes(nested, None)
    try:
        decoded = base64.b64decode(stripped, validate=True)
    except (ValueError, binascii.Error):
        return None
    if decoded[:8] == b"\x89PNG\r\n\x1a\n":
        return decoded
    return None


def _timestamp_run_id() -> str:
    now = datetime.now(timezone.utc)
    return now.strftime("live_%Y%m%dT%H%M%SZ")


def _record_all(out_dir: Path, frame_count: int) -> int:
    notes: list[str] = []
    agent_url = _load_agent_url()
    out_dir.mkdir(parents=True, exist_ok=True)
    run_id = out_dir.name

    tree_text = ""
    captured_frames: list[str] = []
    manifest_views: list[Any] = []

    with AgentClient.connect(agent_url) as client:
        client.call("ping")
        tree_text = _call_ui_text(client, "ui.dump_tree")
        if tree_text.startswith("error:") or _is_unavailable(tree_text):
            notes.append(f"ui.dump_tree: {tree_text[:120]}")
            tree_text = tree_text if tree_text else ""
        else:
            (out_dir / "tree.txt").write_text(tree_text, encoding="utf-8")

        capture_ok = False
        for frame_index in range(frame_count):
            frame_name = f"frame_{frame_index + 1:04d}.png"
            try:
                result, text = _call_ui_raw(
                    client,
                    "ui.capture_shell",
                    {"frame": frame_index, "index": frame_index},
                )
            except AgentError as exc:
                notes.append(f"ui.capture_shell[{frame_index}]: error: {exc}")
                break

            if text is not None and _is_unavailable(text):
                notes.append(f"ui.capture_shell[{frame_index}]: {text.strip()}")
                break

            png = _extract_png_bytes(result, text)
            if png is None:
                note = text.strip() if text else json.dumps(result, ensure_ascii=False)
                notes.append(f"ui.capture_shell[{frame_index}]: no png ({note[:80]})")
                break

            (out_dir / frame_name).write_bytes(png)
            captured_frames.append(frame_name)
            capture_ok = True

            views_payload = result.get("views")
            if isinstance(views_payload, list) and not manifest_views:
                manifest_views = views_payload

        if not capture_ok and tree_text and not (out_dir / "tree.txt").is_file():
            (out_dir / "tree.txt").write_text(tree_text, encoding="utf-8")

    manifest: dict[str, Any] = {
        "run_id": run_id,
        "recorded_at": datetime.now(timezone.utc).isoformat(),
        "mode": "record-all",
        "agent_url": agent_url,
        "frames": captured_frames,
        "notes": notes,
    }
    if manifest_views:
        manifest["views"] = manifest_views

    (out_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    payload = {
        "out_dir": str(out_dir),
        "frames_written": len(captured_frames),
        "notes": notes,
    }
    print(json.dumps(payload, ensure_ascii=False, indent=2))
    if notes and not captured_frames:
        print("note: only ui.dump_tree available; see tree.txt + manifest.json", file=sys.stderr)
    elif notes:
        print("note: partial capture; see manifest notes", file=sys.stderr)
    return 0


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="UI visual forensics (Mode C): analyze runs or live Agent capture."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--analyze",
        metavar="DIR",
        help="Offline analyze a forensics run directory",
    )
    group.add_argument(
        "--record-all",
        action="store_true",
        help="Live DebugAgent capture into out/ui_forensics/live_<timestamp>/",
    )
    group.add_argument(
        "--list-runs",
        action="store_true",
        help="List subdirectories under out/ui_forensics",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=1,
        metavar="N",
        help="Frames to request from ui.capture_shell (with --record-all)",
    )
    parser.add_argument(
        "--out",
        metavar="DIR",
        help="Output directory for --record-all (default: out/ui_forensics/live_<timestamp>/)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    if args.list_runs:
        for name in _list_runs():
            print(name)
        return 0

    if args.analyze:
        run_dir = Path(args.analyze)
        if not run_dir.is_dir():
            raise SystemExit(f"not a directory: {run_dir}")
        summary = _analyze_run(run_dir)
        print(json.dumps(summary, ensure_ascii=False, indent=2))
        return 0 if summary.get("clean") else 1

    if args.record_all:
        if args.frames < 1:
            raise SystemExit("--frames must be >= 1")
        if args.out:
            out_dir = Path(args.out)
        else:
            out_dir = _forensics_root() / _timestamp_run_id()
        return _record_all(out_dir, args.frames)

    raise SystemExit("no action selected")


if __name__ == "__main__":
    raise SystemExit(main())
