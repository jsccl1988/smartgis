# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# debug

Out-of-process Python worker for the SmartGIS Debug Agent. Speaks newline-delimited
JSON (NDJSON) over loopback TCP. Edit-time IntelliSense comes from `smartgis/*.pyi`
+ `pyrightconfig.json`; runtime debugging uses optional **debugpy** and the sample
DAP config under `vscode/launch.json`.

## Layout

| Path | Role |
| --- | --- |
| `worker.py` | `python -m debug.worker` entry |
| `smartgis/client.py` | NDJSON `AgentClient` (events, reconnect) |
| `smartgis/discovery.py` | `%TEMP%/smartgis-debug.json` → connect |
| `smartgis/agent_ui.py` | `ui.*` RPC helpers |
| `smartgis/__init__.py` | Helpers (`log_tail`, `cmd_exec`, `sdbd_query`, `ui_*`, …) |
| `smartgis/*.pyi` | Pyright / LSP stubs (incl. edit-time `ui/` / `gis/` façades) |
| `vscode/launch.json` | Sample attach to debugpy on port **5678** |
| `tests/` | pytest + shared `mock_agent` |
| `scripts/ui_smoke.py` | Live Agent ui.* smoke via discovery |

## Run

From the repository root (so `debug` is importable):

```bat
set PYTHONPATH=tools;tools\debug
python -m debug.worker --agent tcp://127.0.0.1:PORT
```

(`worker` also prepends its package dir so `import smartgis` works when only
`PYTHONPATH=tools` is set.)

Optional DAP listen (install `debugpy` separately):

```bat
python -m debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678
python -m debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678 --wait-for-client
```

Job wall-clock timeout (default 60s; `0` disables):

```bat
python -m debug.worker --agent tcp://127.0.0.1:PORT --job-timeout 30
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
`cmd_exec`, `sdbd_query`, `ui_find`, …), which use `AgentClient.call(method, params)`.

Connect without a hard-coded port:

```python
from smartgis import connect_from_discovery, bind_client, ping
with connect_from_discovery() as client:
    bind_client(client)
    print(ping())
```

**Security note:** `py.run_file` / job `path` runs as the current user. Agent binds
`127.0.0.1` only. This package does not start the Agent. Job `path` must exist and
be a regular file.

## Tests

```bat
set PYTHONPATH=tools
python -m pytest tools/debug/tests -q
```

## Requirements

- Python 3.10+ (3.11 recommended)
- Optional: `debugpy` for `--debugpy`
- Optional: `pytest` for tests; `pyright` for stub checking
