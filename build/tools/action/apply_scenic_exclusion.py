# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""build.bat pre-gen: scenic present host is already in-tree (idempotent).

Historically this rewrote map2d/scene3d presenters via apply_scenic_present_host.py
and set them read-only. That clobbered product fixes (hollow scenic present,
sync tokens) on every build. Sources under src/content/browser/present already
host scenic::Engine behind SMT_*_ENGINE=scenic — do not rewrite them here.
"""
from __future__ import annotations

from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[3]
    map_cc = root / "src/content/browser/present/map2d/map2d_presenter.cc"
    s3_cc = root / "src/content/browser/present/scene3d/scene3d_presenter.cc"
    for p in (map_cc, s3_cc):
        text = p.read_text(encoding="utf-8")
        if "create_map2d_engine" not in text and "create_scene3d_engine" not in text:
            raise SystemExit(
                f"apply_scenic_exclusion: {p} missing scenic host; "
                "restore presenter sources or run apply_scenic_present_host.py once"
            )
    print("apply_scenic_exclusion: content-hosted scenic present already in tree")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
