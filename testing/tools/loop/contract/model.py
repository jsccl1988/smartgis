# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Suite contract types (JSON fields as frozen dataclasses)."""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

from .paths import HARNESS_DIR, ROOT, TOOLS_DIR, resolve_capture_leaf, with_capture_scenario


@dataclass(frozen=True)
class BmpProbe:
    path: str
    score_id: str
    accept_nonzero_rc_if_bmp: bool = False
    min_bytes: int = 10_000
    # When True, china/carto score fail is recorded but does not fail the suite
    # (full Edit chrome BMPs often break showcase china gates).
    soft: bool = False


@dataclass(frozen=True)
class VisualReview:
    """Optional Agent/human visual closed-loop hints (not a te hard gate)."""

    enabled: bool = True
    checklist: tuple[str, ...] = ()
    expect_notes: str = ""


@dataclass(frozen=True)
class ZoomGate:
    """OS wheel zoom-out before/after BMP pixel-diff gate (legacy.browse.2d)."""

    before: str
    after: str
    min_pixel_diff_frac: float = 0.002
    settle_ms: int = 90
    thresh: int = 12


@dataclass(frozen=True)
class MotionGate:
    """Recorded HWND frames must change enough during pan/path (follow-hand)."""

    min_unique_frac: float = 0.25
    min_unique_frames: int = 12
    # Optional: require at least N distinct frame sizes (window resize phases).
    min_unique_sizes: int = 0


@dataclass(frozen=True)
class FpsGate:
    """Soft/hard mean FPS from map2d-fps-bench.txt (browse stutter guard)."""

    report_leaf: str = "map2d-fps-bench.txt"
    min_mean_fps: float = 8.0
    soft: bool = True


@dataclass(frozen=True)
class ClickGate:
    """Require click/dblclick inject + non-black after-click shell BMP."""

    enabled: bool = True
    after: str = "legacy/_click_after.bmp"
    max_near_black: float = 0.55


@dataclass(frozen=True)
class Suite:
    """One outer-loop contract shared with C++ ScenarioRegistry ids."""

    id: str
    kind: str
    argv: list[str]
    env: dict[str, str] = field(default_factory=dict)
    mark_leaf: str | None = None
    required_marks: tuple[str, ...] = ()
    bmp: BmpProbe | None = None
    visual_review: VisualReview | None = None
    zoom_gate: ZoomGate | None = None
    motion_gate: MotionGate | None = None
    fps_gate: FpsGate | None = None
    click_gate: ClickGate | None = None
    # Forgive ExitProcess heap corruption when required marks already landed.
    accept_nonzero_rc_if_marks: bool = False
    rounds: int = 6
    timeout_sec: int = 180
    build_target: str = "src/app/views:views"
    exe_name: str = "SmartGIS.exe"
    report_name: str | None = None
    probes: tuple[str, ...] = ()
    kill_showcase: bool = True
    # Script path relative to suite_dir, or to harness/ (e.g. _shared/...).
    script: str | None = None
    # Directory containing suite.json (for resolving relative script paths).
    suite_dir: Path | None = None
    # inproc (C++ JSON) | os (Python PostMessage/SendInput while exe lingers).
    driver: str = "inproc"
    os_inject_default: str = "postmessage"
    window_title: str = "SmartGIS Views"

    def is_reviewable(self) -> bool:
        """True when BMP probe exists and visual_review is not explicitly disabled."""
        if self.bmp is None:
            return False
        if self.visual_review is None:
            return True
        return bool(self.visual_review.enabled)

    def out_dir(self, config: str = "Debug") -> Path:
        return ROOT / "out" / config

    def captures_root(self, config: str = "Debug") -> Path:
        """captures/ root (record/, analysis/, scenario subdirs)."""
        return self.out_dir(config) / "captures"

    def scenario_dir(self) -> str:
        """Harness family folder (plugin/, ui/, shell/, …)."""
        if self.suite_dir is not None:
            return self.suite_dir.parent.name
        if "." in self.id:
            return self.id.split(".", 1)[0]
        return "shell"

    def captures_dir(self, config: str = "Debug") -> Path:
        """Scenario folder for this suite's marks / BMPs / loop reports."""
        return self.captures_root(config) / self.scenario_dir()

    def exe_path(self, config: str = "Debug") -> Path:
        override = os.environ.get("SMARTGIS_HARNESS_EXE", "").strip()
        if override:
            p = Path(override)
            if not p.is_absolute():
                p = self.out_dir(config) / p
            return p
        return self.out_dir(config) / self.exe_name

    def mark_path(self, config: str = "Debug") -> Path | None:
        if not self.mark_leaf:
            return None
        return resolve_capture_leaf(
            self.captures_root(config), self.mark_leaf, self.scenario_dir()
        )

    def bmp_path(self, config: str = "Debug") -> Path | None:
        if self.bmp is None:
            return None
        return resolve_capture_leaf(
            self.captures_root(config), self.bmp.path, self.scenario_dir()
        )

    def report_path(self, config: str = "Debug") -> Path:
        name = self.report_name or f"{self.id.replace('.', '_')}_loop_report.json"
        rel = with_capture_scenario(name, self.scenario_dir())
        return self.captures_root(config) / Path(rel)

    def script_path(self) -> Path | None:
        if not self.script:
            return None
        p = Path(self.script)
        if p.is_absolute():
            return p
        if self.suite_dir is not None:
            cand = (self.suite_dir / p).resolve()
            if cand.is_file():
                return cand
        cand = (HARNESS_DIR / p).resolve()
        if cand.is_file():
            return cand
        legacy = (TOOLS_DIR / p).resolve()
        if legacy.is_file():
            return legacy
        leaf = p.name
        hits = [
            h
            for h in HARNESS_DIR.rglob(leaf)
            if h.is_file() and h.name == leaf
        ]
        if len(hits) == 1:
            return hits[0]
        return cand
