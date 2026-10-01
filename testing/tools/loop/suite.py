# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Load suite contracts from testing/tools/harness/<family>/<suite_id>/suite.json."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
HARNESS_DIR = TOOLS_DIR / "harness"
SHARED_DIR = HARNESS_DIR / "_shared"
ROOT = TOOLS_DIR.parents[1]

# Top-level dirs under captures/ that are not suite scenario folders.
_CAPTURE_RESERVED_TOP = frozenset({"record", "analysis", "_scratch"})


def capture_scenario_for_leaf(leaf: str) -> str | None:
    """Mirror C++ capture_scenario_prefix_* for flat basenames."""
    name = leaf.replace("\\", "/").strip("/")
    if not name or "/" in name:
        return None
    if name.startswith(("atmosphere-", "atmosphere_")):
        return "atmosphere"
    if name.startswith(("map2d-", "map2d_")):
        return "map2d"
    if name.startswith(("plugin-", "plugin_")):
        return "plugin"
    if name.startswith(("ui-", "ui_")):
        return "ui"
    if name.startswith(("legacy-", "legacy_")):
        return "legacy"
    if name.startswith(
        (
            "input-",
            "input_",
            "self-test-",
            "views-plain-",
            "views_plain_",
            "browse_",
            "browse-",
            "console_",
            "console-",
        )
    ):
        return "shell"
    if name.startswith("_"):
        return "_scratch"
    return None


def with_capture_scenario(leaf: str, family: str | None = None) -> str:
    """Prefix |leaf| with scenario dir when still a flat basename."""
    norm = leaf.replace("\\", "/").lstrip("/")
    if not norm or "/" in norm:
        return norm
    top = norm.split("/", 1)[0]
    if top in _CAPTURE_RESERVED_TOP:
        return norm
    scenario = capture_scenario_for_leaf(norm)
    if scenario is None and family:
        scenario = family
    if scenario:
        return f"{scenario}/{norm}"
    return norm


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
    # Forgive ExitProcess heap corruption when required marks already landed.
    accept_nonzero_rc_if_marks: bool = False
    rounds: int = 6
    timeout_sec: int = 180
    build_target: str = "src/app/views:views"
    exe_name: str = "SmartGisViews.exe"
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
        """Harness family folder (map2d/, plugin/, shell/, …)."""
        if self.suite_dir is not None:
            return self.suite_dir.parent.name
        if "." in self.id:
            return self.id.split(".", 1)[0]
        return "shell"

    def captures_dir(self, config: str = "Debug") -> Path:
        """Scenario folder for this suite's marks / BMPs / loop reports."""
        return self.captures_root(config) / self.scenario_dir()

    def exe_path(self, config: str = "Debug") -> Path:
        import os

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
        root = self.captures_root(config)
        rel = with_capture_scenario(self.mark_leaf, self.scenario_dir())
        primary = root / Path(rel)
        if primary.is_file():
            return primary
        flat = self.mark_leaf.replace("\\", "/").lstrip("/")
        if "/" not in flat:
            alt = root / flat
            if alt.is_file():
                return alt
        return primary

    def bmp_path(self, config: str = "Debug") -> Path | None:
        if self.bmp is None:
            return None
        root = self.captures_root(config)
        rel = with_capture_scenario(self.bmp.path, self.scenario_dir())
        primary = root / Path(rel)
        if primary.is_file():
            return primary
        # Fallback: older binaries wrote flat captures/<leaf> before family
        # subdirs; accept that leaf so loops do not false-fail bmp_missing.
        flat = self.bmp.path.replace("\\", "/").lstrip("/")
        if "/" not in flat:
            alt = root / flat
            if alt.is_file():
                return alt
        return primary

    def report_path(self, config: str = "Debug") -> Path:
        # Canonical: captures/<family>/{suite_id with '.' -> '_'}_loop_report.json
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
        # Legacy: paths written relative to testing/tools/ (harness/...).
        legacy = (TOOLS_DIR / p).resolve()
        if legacy.is_file():
            return legacy
        # Last resort: search harness for the leaf name (skip nothing; unique leaf).
        leaf = p.name
        hits = [
            h
            for h in HARNESS_DIR.rglob(leaf)
            if h.is_file() and h.name == leaf
        ]
        if len(hits) == 1:
            return hits[0]
        return cand


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
        # Optional: family/<id>/<id>.json
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
        kind=str(raw.get("kind", "self_test")),
        argv=[str(a) for a in raw.get("argv", [])],
        env={str(k): str(v) for k, v in (raw.get("env") or {}).items()},
        mark_leaf=raw.get("mark_leaf"),
        required_marks=tuple(str(m) for m in (raw.get("required_marks") or [])),
        bmp=bmp,
        visual_review=visual_review,
        zoom_gate=zoom_gate,
        accept_nonzero_rc_if_marks=bool(
            raw.get("accept_nonzero_rc_if_marks", False)
        ),
        rounds=int(loop.get("rounds", 6)),
        timeout_sec=int(loop.get("timeout_sec", 180)),
        build_target=str(loop.get("build_target", "src/app/views:views")),
        exe_name=str(loop.get("exe", "SmartGisViews.exe")),
        report_name=raw.get("report_name"),
        probes=probe_types,
        kill_showcase=bool(raw.get("kill_showcase", True)),
        script=str(raw["script"]) if raw.get("script") else None,
        suite_dir=path.parent,
        driver=str(raw.get("driver", "inproc")),
        os_inject_default=str(raw.get("os_inject_default", "postmessage")),
        window_title=str(raw.get("window_title", "SmartGIS Views")),
    )
