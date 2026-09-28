# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Ensure package-local ``smartgis`` is importable when PYTHONPATH=python."""

from __future__ import annotations

import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]  # python/smartgis_debug
_PYTHON = _ROOT.parent  # python/
for path in (str(_PYTHON), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)
