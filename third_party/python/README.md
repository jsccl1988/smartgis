<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# CPython 3.12 embed (optional drop-in)

GN auto-detects a machine CPython 3.12 (Windows registry, `py -3.12`,
`%LOCALAPPDATA%\Programs\Python\Python312`, or this directory). Detection lives
in `build/find_python.py` / `build/python.gni`.

## GN args

| Arg | Meaning |
| --- | --- |
| `smt_has_python=true` | Default. Embed when a 3.12 prefix is found; otherwise compile the skip stub. |
| `smt_has_python=false` | Force the stub (`plugin_python_test: skip (no python)`). |
| `smt_python_root="C:/.../Python312"` | Override auto-detect. Prefix must contain `include/Python.h`, `libs/python312.lib`, and `python312.dll`. |

Do **not** vendor a full CPython source tree. Unpack the official
`python-3.12.x-embed-amd64.zip` here only if you need a repo-local prefix:

- `python312.dll`
- `python312.zip`
- `include/Python.h` (from the matching embed/dev package)
- `libs/python312.lib`

`plugin_python_test` prints `plugin_python_test: skip (no python)` and exits 0
when no prefix is found.
