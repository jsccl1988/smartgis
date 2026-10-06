<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Industry pack sample (L3 orchestration)

Template for **kind=python** industry packs under the plugin-host
§product–Python division (P4 skeleton).

## Intent

| Layer | This sample |
| --- | --- |
| L3 | Commands that orchestrate published processing ids |
| L2 | Uses `Host.contribute_command` + `Host.run_processing` |
| L1 | Does **not** implement TIN / GEOS / PROJ kernels |

## Pattern

1. `contribute_command` registers industry menu entries.
2. Handlers call `host.run_processing("world3d.*"| "native.*", args_json)`.
3. Builtin C++ product plugins (`kind=builtin`) stay the **reference UI** and
   performance fallback. Full deletion of builtin dialogs is out of scope;
   withdraw per plugin only when a trusted Python pack replaces that surface.

## Files

| File | Role |
| --- | --- |
| `plugin.json` | Manifest (`kind: python`, command contributions) |
| `plugin.py` | `start(host)` / `stop()` orchestration |

## Related

- Living: `docs/superpowers/specs/2026-09-13-plugin-host-design.md` §product–Python division
- Plan: `docs/superpowers/plans/2026-09-28-gis-python-spatial-analysis.md` follow-on P4
