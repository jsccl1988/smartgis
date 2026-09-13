# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Remove Cursor Co-authored-by trailers from stdin commit message."""
import re
import sys

msg = sys.stdin.read()
lines = msg.splitlines()
filtered = [
    line
    for line in lines
    if not re.match(
        r"^Co-authored-by:\s*(Cursor(\s+Agent)?\s*<|.*cursoragent@cursor\.com)",
        line,
        re.IGNORECASE,
    )
]
while filtered and not filtered[-1].strip():
    filtered.pop()
if filtered:
    sys.stdout.write("\n".join(filtered) + "\n")
