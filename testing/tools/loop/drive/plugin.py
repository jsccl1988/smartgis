# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Patch out/<config>/plugins/<pkg>/plugin.json for harness scenario selection.

Views CLI no longer maps --plugin-showcase / --map2d-showcase onto
ScenarioRegistry. Differentiated harness runs write startup.scenario onto the
copied plugin.json next to the exe (default: flood for chrome-only suites).
"""

from __future__ import annotations

import json
from pathlib import Path

from ..contract import Suite

_RETIRED_PREFIXES = (
    "--plugin-showcase",
    "--map2d-showcase",
    "--atmosphere-showcase",
    "--ui-showcase",
    "--input-showcase",
    "--browse-showcase",
    "--harness-console",
    "--self-test-console",
    "--plugin-present",
    "--atmosphere-fields",
    "--harness",
    "--self-test",
)


def strip_retired_argv(argv: list[str]) -> list[str]:
    out: list[str] = []
    skip_next = False
    for token in argv:
        if skip_next:
            skip_next = False
            continue
        raw = str(token)
        matched = False
        for prefix in _RETIRED_PREFIXES:
            if raw == prefix:
                skip_next = True
                matched = True
                break
            if raw.startswith(prefix + "="):
                matched = True
                break
        if not matched:
            out.append(raw)
    return out


def suite_plugin_package(suite_id: str) -> str:
    sid = str(suite_id)
    if sid.startswith("plugin.flood"):
        return "flood"
    if sid.startswith("plugin.traffic"):
        return "traffic"
    if sid.startswith("plugin.mine"):
        return "mine"
    if sid.startswith("plugin.geochem"):
        return "geochem"
    if sid.startswith("plugin.stormsurge"):
        return "stormsurge"
    if sid.startswith("plugin.report"):
        return "report"
    if sid.startswith("plugin.print") or sid.startswith("plugin.orthogrid"):
        if sid.startswith("plugin.orthogrid3d"):
            return "world3d"
        return "map2d"
    if sid.startswith("plugin.world") or sid.startswith("browser.world3d"):
        return "world3d"
    if sid.startswith("browser.map2d"):
        return "map2d"
    # Chrome / UI / console / input / harness: edit flood plugin.json.
    return "flood"


def _plugins_root(exe: Path) -> Path:
    return exe.resolve().parent / "plugins"


def _load_plugin_json(manifest: Path) -> dict | None:
    """Load plugin.json; tolerate mixed/legacy encodings after corrupt writes."""
    try:
        raw = manifest.read_bytes()
    except OSError as exc:
        print(f"warn: plugin.json read failed {manifest} ({exc})", flush=True)
        return None
    text: str | None = None
    for enc in ("utf-8-sig", "utf-8", "gbk"):
        try:
            text = raw.decode(enc)
            break
        except UnicodeDecodeError:
            continue
    if text is None:
        print(f"warn: plugin.json undecodable {manifest}", flush=True)
        return None
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        print(f"warn: plugin.json parse failed {manifest} ({exc})", flush=True)
        return None
    if not isinstance(data, dict):
        print(f"warn: plugin.json root not object {manifest}", flush=True)
        return None
    return data


def _write_plugin_json(manifest: Path, data: dict) -> None:
    manifest.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def apply_suite_plugin_startup(suite: Suite, exe: Path) -> Path | None:
    root = _plugins_root(exe)
    if not root.is_dir():
        print(f"warn: plugins dir missing ({root})", flush=True)
        return None
    target_pkg = suite_plugin_package(suite.id)
    target_path: Path | None = None
    for pkg_dir in sorted(root.iterdir()):
        if not pkg_dir.is_dir():
            continue
        manifest = pkg_dir / "plugin.json"
        if not manifest.is_file():
            continue
        data = _load_plugin_json(manifest)
        if data is None:
            continue
        startup = data.get("startup")
        if not isinstance(startup, dict):
            startup = {}
        if pkg_dir.name == target_pkg:
            startup["activate"] = True
            startup["scenario"] = suite.id
            target_path = manifest
        else:
            startup["activate"] = False
            startup.pop("scenario", None)
        data["startup"] = startup
        _write_plugin_json(manifest, data)
    if target_path is None:
        print(
            f"warn: no plugin.json for package {target_pkg} under {root}",
            flush=True,
        )
        return None
    print(f"plugin.json startup: {target_path} scenario={suite.id}", flush=True)
    return target_path


def restore_product_plugin_startup(exe: Path) -> None:
    root = _plugins_root(exe)
    if not root.is_dir():
        return
    for pkg_dir in root.iterdir():
        if not pkg_dir.is_dir():
            continue
        manifest = pkg_dir / "plugin.json"
        if not manifest.is_file():
            continue
        data = _load_plugin_json(manifest)
        if data is None:
            continue
        startup = data.get("startup")
        if not isinstance(startup, dict):
            continue
        startup["activate"] = True
        startup.pop("scenario", None)
        data["startup"] = startup
        _write_plugin_json(manifest, data)
