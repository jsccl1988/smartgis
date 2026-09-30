<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/plugin` subdirectory layout (runtime + product) — Implementation Plan

> **For agentic workers:** Execute on **master**. Layout only; no ABI / dll_stem change. No shim headers.

**Goal:** Align leftover `src/legacy/plugin` with endgame `src/plugin/{runtime,product}` **without leaving `legacy/`**. `runtime/{auxmodule,bridge}` + `product/<domain>/{shell,views}` (+ `kernel/` for orthogrid). Do not move AuxModule / MFC shells into `src/plugin/`.

**Living:** [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) § Leftover `legacy/plugin` role layout.

## Target

```
legacy/plugin/
  BUILD.gn  README.md
  runtime/
    auxmodule/          # SmtAuxModule → :plugin DLL
    bridge/             # am.cc + cmd.h (header-only) → :bridge
  product/<domain>/
    BUILD.gn
    shell/
    views/
    res/
    kernel/             # orthogrid only
```

## Tasks

- [x] `git mv` AuxModule root → `module/` then `runtime/module/` then flatten then `runtime/auxmodule/`
- [x] `git mv` adapter → `runtime/adapter/` → `runtime/am/` → flatten → `runtime/bridge/` (`cmd.h` header-only)
- [x] `git mv` each domain → `product/<domain>/` with `shell/` / `views/`
- [x] Update BUILD.gn `sources` / labels (`//src/legacy/plugin/runtime:bridge`, `//src/legacy/plugin/product/<domain>`)
- [x] Scheme C include sweep; no shim
- [x] README + `docs/build/src-layout.md` + living § + Active date
- [x] `build.bat debug` `src/legacy/plugin:plugin` green
- [ ] `src/legacy/plugin:am_plugins` — blocked by unrelated `gis_d.dll` / `legacy_tool_d.dll` LNK (SmtListener / OgrRasterLayer), not include paths

## Done when

- [x] Root has only BUILD + README + `runtime/` + `product/`
- [x] Zero old includes (`legacy/plugin/module/`, `legacy/plugin/adapter/`, `legacy/plugin/runtime/adapter/`, `legacy/plugin/runtime/am/`, `legacy/plugin/runtime/module/`, flat `runtime/{am,cmd,module}.*`, `legacy/plugin/dem/` without `product/`)
- [x] `dll_stem` / DEF / `Smt_*` unchanged
- [ ] Build green
