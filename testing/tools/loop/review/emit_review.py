# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Emit ``*_visual_review.json`` stubs for the Agent/human closed-loop."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from ..contract import Suite, with_capture_scenario
from .inspect_png import bmp_to_inspect_png, inspect_png_path


def review_json_path(suite: Suite, config: str = "Debug") -> Path:
    """``captures/<family>/{suite_id with '.' -> '_'}_visual_review.json``."""
    name = f"{suite.id.replace('.', '_')}_visual_review.json"
    rel = with_capture_scenario(name, suite.scenario_dir())
    return suite.captures_root(config) / Path(rel)


def emit_visual_review(
    suite: Suite,
    *,
    config: str = "Debug",
    score: dict[str, Any] | None = None,
    status: str = "pending",
    bmp_path: Path | None = None,
    write_inspect: bool = True,
) -> Path:
    """Write review stub (+ optional inspect PNG). Returns JSON path.

    Does not invent ``bugs[]`` — Agent/human fill those after confirm.
    """
    captures = suite.captures_dir(config)
    captures.mkdir(parents=True, exist_ok=True)

    bmp = bmp_path
    if bmp is None and suite.bmp is not None:
        bmp = suite.bmp_path(config)

    inspect_rel = ""
    bmp_rel = ""
    if bmp is not None and bmp.is_file():
        try:
            bmp_rel = bmp.relative_to(suite.captures_root(config)).as_posix()
        except ValueError:
            bmp_rel = bmp.name
        png = inspect_png_path(bmp)
        if write_inspect:
            png = bmp_to_inspect_png(bmp, dest=png)
        if png.is_file():
            try:
                inspect_rel = png.relative_to(suite.captures_root(config)).as_posix()
            except ValueError:
                inspect_rel = png.name

    vr = suite.visual_review
    checklist: list[str] = list(vr.checklist) if vr is not None else []
    expect_notes = vr.expect_notes if vr is not None else ""

    score_id = suite.bmp.score_id if suite.bmp is not None else ""
    payload: dict[str, Any] = {
        "suite_id": suite.id,
        "status": status,
        "bmp": bmp_rel,
        "inspect_png": inspect_rel,
        "score_id": score_id,
        "score": score if score is not None else {},
        "checklist": checklist,
        "expect_notes": expect_notes,
        "bugs": [],
    }

    path = review_json_path(suite, config)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return path


def maybe_emit_visual_review(
    suite: Suite,
    *,
    config: str,
    result: dict,
    bmp_path: Path | None,
    force: bool = False,
) -> Path | None:
    """Write inspect PNG + pending review JSON when suite is reviewable."""
    if not force and not suite.is_reviewable():
        return None
    if suite.bmp is None:
        return None
    score = result.get("bmp") if isinstance(result.get("bmp"), dict) else None
    if score is None and isinstance(result.get("gates"), dict) and "ok" in result:
        score = {
            k: v
            for k, v in result.items()
            if k
            not in (
                "suite",
                "visual_review",
                "round",
                "driver",
                "t_ms",
                "steps",
                "os_inject",
                "record",
                "record_path",
                "bmp_age_s",
                "marks",
                "missing",
                "error",
                "rc_forgiven",
                "showcase_rc",
            )
        }
    try:
        path = emit_visual_review(
            suite,
            config=config,
            score=score,
            status="pending",
            bmp_path=bmp_path,
            write_inspect=True,
        )
    except Exception as exc:  # noqa: BLE001
        print(f"warn: visual_review emit failed ({exc})", flush=True)
        return None
    result["visual_review"] = {
        "path": str(path),
        "status": "pending",
        "inspect_png": (bmp_path.stem + ".inspect.png") if bmp_path else "",
    }
    print(f"visual_review: {path}", flush=True)
    return path