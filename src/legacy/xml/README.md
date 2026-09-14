<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/xml`

TinyXML sources moved out of `src/base/core` (Phase 6 of base-root-hybrid).

| Path | Role |
| --- | --- |
| `xml.h` / `xml.cpp` | TinyXML DOM |
| `xmlparser.cpp` / `xmlerror.cpp` | Parser + error strings |

Include: `#include "legacy/xml/xml.h"`. Still linked into the product platform DLL via `xml_sources` → `//src/base:base`. No active product call sites found beyond these TUs; safe to delete once unused.
