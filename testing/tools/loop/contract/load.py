# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Discover and parse harness/<family>/<id>/suite.json."""

from __future__ import annotations

import json
from pathlib import Path

from .model import (
    BmpProbe,
    ClickGate,
    FpsGate,
    MotionGate,
    Suite,
    VisualReview,
    ZoomGate,
)
from .paths import HARNESS_DIR, SHARED_DIR


def _under_shared(path: Path) -> bool:
    try:
        path.relative_to(SHARED_DIR)
        return True
    except ValueError:
        return False


def _suite_json_paths() -> list[Path]:
    """Discover suite.json (or {id}.json whose parent dir name matches stem)."""
    if not HARNESS_DIR.is_dir():
        return []
    out: list[Path] = []
    for p in HARNESS_DIR.rglob("*.json"):
        if _under_shared(p):
            continue
        if p.name == "suite.json":
            out.append(p)
            continue
        if p.parent.name == p.stem:
            out.append(p)
    return sorted(out)


def _suite_id_from_path(path: Path) -> str:
    if path.name == "suite.json":
        return path.parent.name
    return path.stem


def find_suite_path(suite_id: str) -> Path | None:
    """Resolve harness/<family>/<suite_id>/suite.json (or matching {id}.json)."""
    matches = [p for p in _suite_json_paths() if _suite_id_from_path(p) == suite_id]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        raise FileNotFoundError(
            f"ambiguous suite id {suite_id}: {', '.join(str(p) for p in matches)}"
        )
    return None


def list_suite_ids() -> list[str]:
    return sorted({_suite_id_from_path(p) for p in _suite_json_paths()})


def load_suite(suite_id: str) -> Suite:
    path = find_suite_path(suite_id)
    if path is None or not path.is_file():
        known = ", ".join(list_suite_ids()) or "(none)"
        raise FileNotFoundError(f"suite not found: {suite_id} (known: {known})")
    raw = json.loads(path.read_text(encoding="utf-8-sig"))
    json_id = str(raw.get("id", ""))
    if json_id and json_id != suite_id:
        raise ValueError(
            f"suite id mismatch: path implies {suite_id!r} but json id={json_id!r} ({path})"
        )
    loop = raw.get("loop") or {}
    probes_raw = raw.get("probes")
    bmp_raw = raw.get("bmp")
    bmp: BmpProbe | None = None
    if isinstance(bmp_raw, dict) and bmp_raw.get("path") and bmp_raw.get("score_id"):
        bmp = BmpProbe(
            path=str(bmp_raw["path"]),
            score_id=str(bmp_raw["score_id"]),
            accept_nonzero_rc_if_bmp=bool(
                bmp_raw.get("accept_nonzero_rc_if_bmp", False)
            ),
            min_bytes=int(bmp_raw.get("min_bytes", 10_000)),
            soft=bool(bmp_raw.get("soft", False)),
        )

    visual_review: VisualReview | None = None
    vr_raw = raw.get("visual_review")
    if isinstance(vr_raw, dict):
        checklist_raw = vr_raw.get("checklist") or []
        visual_review = VisualReview(
            enabled=bool(vr_raw.get("enabled", True)),
            checklist=tuple(str(c) for c in checklist_raw),
            expect_notes=str(vr_raw.get("expect_notes") or ""),
        )

    zoom_gate: ZoomGate | None = None
    zg_raw = raw.get("zoom_gate")
    if isinstance(zg_raw, dict) and zg_raw.get("before") and zg_raw.get("after"):
        zoom_gate = ZoomGate(
            before=str(zg_raw["before"]),
            after=str(zg_raw["after"]),
            min_pixel_diff_frac=float(zg_raw.get("min_pixel_diff_frac", 0.002)),
            settle_ms=int(zg_raw.get("settle_ms", 90)),
            thresh=int(zg_raw.get("thresh", 12)),
        )

    motion_gate: MotionGate | None = None
    mg_raw = raw.get("motion_gate")
    if isinstance(mg_raw, dict):
        motion_gate = MotionGate(
            min_unique_frac=float(mg_raw.get("min_unique_frac", 0.25)),
            min_unique_frames=int(mg_raw.get("min_unique_frames", 12)),
            min_unique_sizes=int(mg_raw.get("min_unique_sizes", 0)),
        )

    fps_gate: FpsGate | None = None
    fg_raw = raw.get("fps_gate")
    if isinstance(fg_raw, dict):
        fps_gate = FpsGate(
            report_leaf=str(fg_raw.get("report_leaf") or "map2d-fps-bench.txt"),
            min_mean_fps=float(fg_raw.get("min_mean_fps", 8.0)),
            soft=bool(fg_raw.get("soft", True)),
        )

    click_gate: ClickGate | None = None
    cg_raw = raw.get("click_gate")
    if isinstance(cg_raw, dict) and bool(cg_raw.get("enabled", True)):
        click_gate = ClickGate(
            enabled=True,
            after=str(cg_raw.get("after") or "legacy/_click_after.bmp"),
            max_near_black=float(cg_raw.get("max_near_black", 0.55)),
        )

    if probes_raw is None:
        types: list[str] = []
        if raw.get("required_marks"):
            types.append("marks")
        if bmp is not None:
            types.append("bmp")
        probe_types = tuple(types)
    else:
        probe_types = tuple(
            p if isinstance(p, str) else str(p.get("type", ""))
            for p in probes_raw
            if (isinstance(p, str) and p)
            or (isinstance(p, dict) and p.get("type"))
        )

    return Suite(
        id=str(raw["id"]),
        kind=str(raw.get("kind", "harness")),
        argv=[str(a) for a in raw.get("argv", [])],
        env={str(k): str(v) for k, v in (raw.get("env") or {}).items()},
        mark_leaf=raw.get("mark_leaf"),
        required_marks=tuple(str(m) for m in (raw.get("required_marks") or [])),
        bmp=bmp,
        visual_review=visual_review,
        zoom_gate=zoom_gate,
        motion_gate=motion_gate,
        fps_gate=fps_gate,
        click_gate=click_gate,
        accept_nonzero_rc_if_marks=bool(
            raw.get("accept_nonzero_rc_if_marks", False)
        ),
        rounds=int(loop.get("rounds", 6)),
        timeout_sec=int(loop.get("timeout_sec", 180)),
        build_target=str(loop.get("build_target", "src/app/views:views")),
        exe_name=str(loop.get("exe", "SmartGIS.exe")),
        report_name=raw.get("report_name"),
        probes=probe_types,
        kill_showcase=bool(raw.get("kill_showcase", True)),
        script=str(raw["script"]) if raw.get("script") else None,
        suite_dir=path.parent,
        driver=str(raw.get("driver", "inproc")),
        os_inject_default=str(raw.get("os_inject_default", "postmessage")),
        window_title=str(raw.get("window_title", "SmartGIS Views")),
    )
