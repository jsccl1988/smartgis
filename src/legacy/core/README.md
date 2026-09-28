<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/core`

Leftover Smt core (listener / command / msg / api / structs / assert / `core.h`)
moved out of `src/base/core` so `src/base` stays foundation-only.

| Path | Role |
| --- | --- |
| `core.h` | Legacy Smt macros / `SmtErr` / TRACE helpers |
| `api.*` | Variant / string / geometry helpers (`BASE_EXPORT`) |
| `listener*` / `listenermanager*` | Listener registration |
| `command*` | Command stack |
| `msg*` / `msg_def.h` | Message ids / dispatch |
| `bas_struct.h` / `env_struct.h` | POD structs (`SmtVariant`, tiles, …) |
| `core_assert*` / `core_exception.h` | Legacy assert / exception |

Include: `#include "legacy/core/api.h"` (etc.). Foundation keepers remain at
`#include "base/core/export.h"` / `log.h` / …. Still linked into the product
platform DLL via `core_sources` → `//src/base:base` (same pattern as
`legacy/xml` and `legacy/sys`).
