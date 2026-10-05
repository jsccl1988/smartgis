# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""build.bat pre-gen: scenic present host is already in-tree (idempotent).

Historically this rewrote map2d/scene3d presenters via apply_scenic_present_host.py
and set them read-only. That clobbered product fixes (hollow scenic present,
sync tokens) on every build. Sources under src/content/browser/present already
host scenic::Engine behind SCENE3D_ENGINE / MAP2D_ENGINE=scenic -- do not rewrite them here.
"""
from __future__ import annotations

from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    map_cc = root / "src/content/browser/present/map2d/map2d_presenter.cc"
    s3_cc = root / "src/content/browser/present/scene3d/scene3d_presenter.cc"
    s3_host = root / "src/content/browser/present/scene3d/scenic_engine_host.cc"
    map_text = map_cc.read_text(encoding="utf-8")
    if "create_map2d_engine" not in map_text:
        raise SystemExit(
            f"apply_scenic_exclusion: {map_cc} missing scenic host; "
            "restore presenter sources or run apply_scenic_present_host.py once"
        )
    s3_text = s3_cc.read_text(encoding="utf-8")
    s3_host_text = s3_host.read_text(encoding="utf-8") if s3_host.is_file() else ""
    if "scenic_host_" not in s3_text and "create_scene3d_engine" not in s3_text:
        raise SystemExit(
            f"apply_scenic_exclusion: {s3_cc} missing scenic host; "
            "restore presenter sources or run apply_scenic_present_host.py once"
        )
    if "create_scene3d_engine" not in s3_text and "create_scene3d_engine" not in s3_host_text:
        raise SystemExit(
            f"apply_scenic_exclusion: {s3_host} missing scenic host; "
            "restore presenter sources or run apply_scenic_present_host.py once"
        )
    print("apply_scenic_exclusion: content-hosted scenic present already in tree")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
