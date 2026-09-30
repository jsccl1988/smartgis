<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool` endgame-mirror layout

**Goal:** Eliminate `group/` / `iatool` / `adapter` / `bridge` shell names; mirror endgame modules (`nav`←view, `draft`←input); keep leftover-only `abi` + `msg` as top-level with two GN targets.

**Umbrella:** [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §SP1 Layout.

## Checklist

- [x] Hoist `group/` capabilities; delete `group/` shell
- [x] `iatool`→`abi`, `adapter`→`msg` (flatten out of `bridge/`)
- [x] `view`→`nav`, `input`→`draft` (endgame shallow mirror)
- [x] Fold `tool_group_sources` into `//src/legacy/tool:tool_group_sources`
- [x] Break includes / GN deps (no shim)
- [x] Update as-built README + `docs/build/src-layout.md` + living §SP1
- [x] `ninja` `legacy_tool` + `ui_legacy`
