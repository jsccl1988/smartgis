#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
git filter-branch -f --msg-filter "python .superpowers/sdd/strip_cursor_trailer.py" origin/master..HEAD
