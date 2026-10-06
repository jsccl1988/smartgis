# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Legacy thin-wrapper entry (removed). Prefer loop_runner --suite <id>.

Harness suites are suite.json + optional *.il only. Colocated *_loop.py
aliases under testing/tools/harness/ were deleted; use:

  py -3 testing/tools/loop_runner.py --suite <id> [--no-build]
"""

from __future__ import annotations

import sys


def main_for_script(script_file: str, argv: list[str] | None = None) -> int:
    _ = argv
    print(
        f"removed thin wrapper {script_file}; "
        "use: py -3 testing/tools/loop_runner.py --suite <id>",
        file=sys.stderr,
    )
    return 2
