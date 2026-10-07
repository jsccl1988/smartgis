---
name: docs-portal
description: >-
  Opens the SmartGIS local docs portal (sidebar catalog + MD/HTML preview)
  using docs/portal/open.bat. Use when the user asks to open docs preview,
  docs portal, 打开 docs preview, 打开文档预览, 打开门户, /docs-portal, or
  @docs/portal.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Docs portal preview

**用 `docs/portal/open.bat` 打开本地 docs 门户**（侧栏目录 + Markdown / HTML 预览）。不要自己起静态文件服务器。

## Authorization

When this skill is invoked, attached (`@docs-portal` / `/docs-portal` / `@docs/portal`), or the user asks to open docs preview, **start the portal and show it** — do not only print the command.

## Hard rules

1. **Entry:** from the **smartgis repo root**, run `docs\portal\open.bat`. Forward extra args as-is (`%*` → `serve.py`).
2. **Agent loop:** `docs\portal\open.bat --no-browser` in the **background** (the bat blocks until Ctrl+C). Parse stdout for `url` (`http://127.0.0.1:<port>/`). Then open that URL in the Cursor IDE browser (`position: "side"` when the user said 打开 / preview / 预览).
3. **Do not** `python -m http.server` / `py -3 -m http.server --directory docs`. That is a directory listing, not the portal. `/` must be `SmartGIS Docs Portal` (`docs/portal/index.html`).
4. **Do not** `file://` open `docs/portal/index.html` (needs `GET /api/catalog`). **Do not** invent a second port unless `open.bat` / `serve.py` already printed one (preferred **8765**; busy → auto-picks).
5. Confirm: `GET /api/catalog` returns JSON with `counts`. If the body is `Directory listing for /`, a stale static server still owns the port — kill those **LISTENING** PIDs, then re-run `open.bat`.
6. Deep link (optional): `http://127.0.0.1:<port>/?path=<rel under docs/>` e.g. `?path=superpowers/diagrams/content-embedder.html`. If the user had a `docs/` file focused, pass that relative path.

```bat
docs\portal\open.bat --no-browser
```

Human one-shot (also opens the OS default browser):

```bat
docs\portal\open.bat
```

## Start loop

1. Probe `http://127.0.0.1:8765/api/catalog`. If JSON with `counts` → already running; skip start; open `http://127.0.0.1:8765/` (plus `?path=` if needed).
2. If `/` is a directory listing or `/api/catalog` is 404: `netstat -ano` for `:8765` **LISTENING**, `Stop-Process` those PIDs (non-sandbox so the signal lands). Then start `docs\portal\open.bat --no-browser`.
3. Await stdout: `url      http://127.0.0.1:<port>/`.
4. Navigate the Cursor browser to that URL. Title must be **SmartGIS Docs Portal**, not a folder index.
5. Tell the user the URL in one line.

## Files

| Path | Role |
| --- | --- |
| `docs/portal/open.bat` | Windows launcher (`py -3` then `python`) |
| `docs/portal/serve.py` | Scan `docs/` + HTTP |
| `docs/portal/index.html` | UI (no CDN) |
| `docs/portal/README.md` | Human notes |

Python: `py -3` (this machine often has no `python` on PATH). `open.bat` already picks `py` then `python`.
