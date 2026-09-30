# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Load suite contracts from testing/tools/suites/*.json."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
SUITES_DIR = TOOLS_DIR / "suites"
ROOT = TOOLS_DIR.parents[1]


@dataclass(frozen=True)
class BmpProbe:
    path: str
    score_id: str
    accept_nonzero_rc_if_bmp: bool = False
    min_bytes: int = 10_000


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
    rounds: int = 6
    timeout_sec: int = 180
    build_target: str = "src/app/views:views"
    exe_name: str = "SmartGisViews.exe"
    report_name: str | None = None
    probes: tuple[str, ...] = ()
    kill_showcase: bool = True

    def out_dir(self, config: str = "Debug") -> Path:
        return ROOT / "out" / config

    def exe_path(self, config: str = "Debug") -> Path:
        return self.out_dir(config) / self.exe_name

    def mark_path(self, config: str = "Debug") -> Path | None:
        if not self.mark_leaf:
            return None
        return self.out_dir(config) / self.mark_leaf

    def bmp_path(self, config: str = "Debug") -> Path | None:
        if self.bmp is None:
            return None
        return self.out_dir(config) / self.bmp.path

    def report_path(self, config: str = "Debug") -> Path:
        name = self.report_name or f"{self.id.replace('.', '_')}_loop_report.json"
        return self.out_dir(config) / name


def list_suite_ids() -> list[str]:
    if not SUITES_DIR.is_dir():
        return []
    return sorted(p.stem for p in SUITES_DIR.glob("*.json"))


def load_suite(suite_id: str) -> Suite:
    path = SUITES_DIR / f"{suite_id}.json"
    if not path.is_file():
        known = ", ".join(list_suite_ids()) or "(none)"
        raise FileNotFoundError(f"suite not found: {suite_id} (known: {known})")
    raw = json.loads(path.read_text(encoding="utf-8"))
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
        rounds=int(loop.get("rounds", 6)),
        timeout_sec=int(loop.get("timeout_sec", 180)),
        build_target=str(loop.get("build_target", "src/app/views:views")),
        exe_name=str(loop.get("exe", "SmartGisViews.exe")),
        report_name=raw.get("report_name"),
        probes=probe_types,
        kill_showcase=bool(raw.get("kill_showcase", True)),
    )
