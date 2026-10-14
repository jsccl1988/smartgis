# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""DISABLED — do not rewrite presenters.

Historically this script overwrote map2d/scene3d presenter sources with an
outdated soft-present (software/) template and set them read-only, clobbering
product fixes on every accidental run.

Product presenters already host scenic::Engine behind MAP2D_ENGINE /
SCENE3D_ENGINE=scenic. Soft present directories are deleted. Directory layout is
locked in src/content/browser/present/README.md and living
§Content present subdirectory layers.

Use apply_scenic_exclusion.py (pre-gen check only). Do not resurrect this
writer.
"""

from __future__ import annotations

import sys


def main() -> int:
    print(
        "apply_scenic_present_host.py is retired. "
        "It must not rewrite src/content/browser/present/**. "
        "See apply_scenic_exclusion.py and present/README.md.",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
