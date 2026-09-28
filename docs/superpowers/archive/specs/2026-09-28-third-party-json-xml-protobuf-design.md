<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-14-base-root-hybrid-design.md`](../../specs/2026-09-14-base-root-hybrid-design.md) — §third-party JSON/XML/protobuf (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# Third-party JSON / XML / protobuf (no in-tree parsers)

**Date:** 2026-09-28  
**Status:** superseded (2026-09-28 merge B)
**Updated:** 2026-09-28  

**Living files considered:** [`2026-09-14-sdb-style-document-design.md`](2026-09-14-sdb-style-document-design.md) (hand-rolled `json_mini`), [`2026-09-13-base-ipc-mojom-design.md`](2026-09-13-base-ipc-mojom-design.md) / [`2026-09-13-base-archive-design.md`](2026-09-13-base-archive-design.md) (`No protobuf` for IPC), [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md) (MVT + `json_mini`).  
**Why a new dated file:** incompatible policy fork — product code must **not** ship homemade JSON/XML/protobuf codecs; prior living specs explicitly forbade a third JSON library and banned protobuf for IPC. Folding into one of those would force contradictory “non-goals” in the same document.

**Plan:** [`../plans/2026-09-28-third-party-json-xml-protobuf.md`](../plans/2026-09-28-third-party-json-xml-protobuf.md)

## Goal

- **JSON:** RapidJSON only (`third_party`). Delete `json_mini` and every hand-rolled `parse_json` / `Json` tree / `json_escape` / `json_get_*` used as a parser or encoder.
- **XML:** pugixml only. Remove vendored TinyXML under `src/legacy/xml` (no product callers).
- **Protobuf:** official `protobuf` in `third_party` manifest + GN. Policy allows product and future IPC use; **this slice does not change BinarySink wire**.

## Non-goals (this slice)

- Do not migrate named-pipe / FnRPC / `base::archive` BinarySink to protobuf yet.
- Do not implement MVT decode beyond existing stub (protobuf pin enables the next tile slice).
- Do not add a product `base::json` / `base::xml` facade that reimplements parsing.

## Pins

| Package | Role | Wire-up |
| --- | --- | --- |
| RapidJSON | All JSON parse/write | Header-only `install_skip`; `//third_party:rapidjson` |
| pugixml | All XML | Source compile or install; `//third_party:pugixml` |
| protobuf | Allowed codec; MVT / new protocols | Manifest + GN; consumers opt in |

## Call-site policy

- Prefer `#include <rapidjson/document.h>` / `writer.h` / `stringbuffer.h` directly.
- Prefer `#include <pugixml.hpp>` directly.
- Prefer generated / `google/protobuf` APIs for protobuf.
- Chrome Trace and similar emitters use RapidJSON `Writer`, not local escape helpers.

## Amendments to older living text

- Style design: drop “不引入第三套 JSON 库 / json_mini”; reuse RapidJSON.
- Tile sources: same — no `json_mini`.
- IPC / archive: keep BinarySink for current wire; **policy** now allows protobuf for new protocols and a future IPC migration (separate plan).

## Acceptance

- [x] No `json_mini` sources in tree.
- [x] No TinyXML TUs in `base` DLL.
- [x] `//third_party:{rapidjson,pugixml,protobuf}` resolve when pins are fetched.
- [x] Style / sdbd / plugin / catalog JSON paths compile and existing unit tests pass.
- [x] `build.bat` green for touched targets.
