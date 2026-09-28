# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# smartgis_debug

Out-of-process Python worker for the SmartGIS Debug Agent. Speaks newline-delimited
JSON (NDJSON) over loopback TCP. Edit-time IntelliSense comes from `smartgis/*.pyi`
+ `pyrightconfig.json`; runtime debugging uses optional **debugpy** and the sample
DAP config under `vscode/launch.json`.

## Layout

| Path | Role |
| --- | --- |
| `worker.py` | `python -m smartgis_debug.worker` entry |
| `smartgis/client.py` | Thin NDJSON `AgentClient` |
| `smartgis/__init__.py` | Helpers (`log_tail`, `cmd_exec`, `sdbd_query`, …) |
| `smartgis/*.pyi` | Pyright / LSP stubs |
| `vscode/launch.json` | Sample attach to debugpy on port **5678** |
| `tests/` | pytest for framing + mock Agent |

## Run

From the repository root (so `smartgis_debug` is importable):

```bat
set PYTHONPATH=python;python\smartgis_debug
python -m smartgis_debug.worker --agent tcp://127.0.0.1:PORT
```

(`worker` also prepends its package dir so `import smartgis` works when only
`PYTHONPATH=python` is set.)

Optional DAP listen (install `debugpy` separately):

```bat
python -m smartgis_debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678
python -m smartgis_debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678 --wait-for-client
```

Copy or merge `vscode/launch.json` into your workspace `.vscode/launch.json` to
attach VS Code to that port. Do not clobber an existing user launch config blindly.

## Protocol (v1 persistent worker)

1. Worker connects to Agent as a **client**.
2. Sends `{ "id":1, "method":"py.register", "params":{} }` and waits for `ok`.
3. Agent pushes jobs on the same connection:

   `{ "id":N, "method":"py.job", "params":{ "id":"<job>", "code":"…" } }`  
   or `{ "path":"…" }` instead of `code`.

4. Worker replies with `{ "id":N, "ok":true, "result":{ "stdout", "stderr", … } }`  
   or `{ "id":N, "ok":false, "error":"…" }`.

Scripts may call back into the product via `smartgis` helpers (`ping`, `log_tail`,
`cmd_exec`, `sdbd_query`, …), which use `AgentClient.call(method, params)`.

**Security note:** `py.run_file` / job `path` runs as the current user. Agent binds
`127.0.0.1` only. This package does not start the Agent.

## Tests

```bat
set PYTHONPATH=python
python -m pytest python/smartgis_debug/tests -q
```

## Requirements

- Python 3.10+ (3.11 recommended)
- Optional: `debugpy` for `--debugpy`
- Optional: `pytest` for tests; `pyright` for stub checking
