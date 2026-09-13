<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# mapd HTTP client Implementation Plan

**Status:** superseded — product web / mapd stack deleted 2026-09-13. Do not implement.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `web::MapdClient` so this repo can issue mapd HTTP requests on `:8020` (health / style / capabilities / Path Q / tile URLs) with fake-transport tests.

**Architecture:** Injectable `HttpTransport`. Fake for contracts; `net::HttpClient` for live. No sdbd, no GDAL remote open, no leftover WMS rewrite.

**Tech Stack:** C++23, `source_set` under `src/web/mapd`, `net::HttpClient`, `expect()` tests.

## Global Constraints

- Stay on `master`; do not create topic branches; do not commit unless the user asks.
- Copyright: `// Copyright (c) 2026 The Mogu Authors.` on every new/touched engineering file.
- New C++: `snake_case` functions; public namespace `web`; internals in `web::detail`.
- `cc_std` is the repo default (`c++23` / MSVC `/std:c++23preview`).
- Comments in English.
- mapd only: reject `:8021`; no `SdbdClient`; no `sql` fields on `QueryRequest`.
- Do not edit `src/ui/xview`, `src/plugin/map_service`, or delete leftover `src/web` servers.
- Qt banned. Build output only `out/` via `build.bat` / `ninja -C out`.
- No `Co-authored-by: Cursor`.

---

### Task 1: QueryRequest + link parse + fake client (tests first)

**Files:**
- Create: everything under `src/web/mapd/` listed in the spec file map
- Modify: `src/web/BUILD.gn`, `src/BUILD.gn`, `BUILD.gn`, `src/README.md`, `docs/README.md`, `docs/build/src-layout.md`

**Interfaces:**
- Produces: `web::MapdClient`, `web::QueryRequest::to_json`, `web::parse_mapd_link`, `web::parse_capabilities`

- [ ] **Step 1:** Write `mapd_client_test.cc` covering the spec table (link, to_json, fake ready, 503, tile URL, optional live).
- [ ] **Step 2:** Implement transport, JSON lite, client, link parse, NetHttpTransport.
- [ ] **Step 3:** Wire GN + docs. Run `ninja -C out mapd_client_test` then `out\mapd_client_test.exe`.

Expected: exit 0, no FAIL lines.

---

## Self-review

1. Spec coverage: client methods, parse, errors, tests, docs — Task 1.
2. No TBD. Live mapd is skip-by-default, not a placeholder API.
3. Names: `health` / `get_style` / `get_capabilities` / `query` / `tile_url` / `parse_mapd_link`.
