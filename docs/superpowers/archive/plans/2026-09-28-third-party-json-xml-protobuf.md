<!--

Copyright (c) 2026 The Mogu Authors.

All rights reserved.

-->



# Third-party JSON / XML / protobuf — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)



> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or execute tasks in parallel on `master` with disjoint paths.



**Goal:** Pin RapidJSON + pugixml + protobuf; delete homemade JSON/XML codecs; leave IPC BinarySink unchanged this slice.



**Architecture:** Direct third-party includes at call sites. No product parser facade.



**Tech Stack:** GN/`build.bat`, `third_party/manifest.json`, RapidJSON, pugixml, protobuf.



**Spec:** `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`



## Global Constraints



- Stay on `master`.

- Copyright 2026 Mogu on new/touched engineering files.

- Qt banned. Output `out/` only via `build.bat`.

- Do not change IPC BinarySink wire in this plan.

- Comments in English.



## File map



| Path | Responsibility |

| --- | --- |

| `third_party/manifest.json` | Pins for rapidjson, pugixml, protobuf |

| `third_party/rapidjson/BUILD.gn` | Header-only include config |

| `third_party/pugixml/BUILD.gn` | Compile pugixml.cpp |

| `third_party/protobuf/BUILD.gn` + `third_party/gn/BUILD.gn` | GN groups / install configs |

| `build/smartgis.gni` | `smt_has_*` probes |

| `src/gis/style/**` | Drop `json_mini`; RapidJSON |

| `src/gis/datasource/provider/impl/sdbd/codec/**` | Drop homemade Json; RapidJSON |

| `src/plugin/runtime/host/manifest/manifest.cc` | Drop homemade Json; RapidJSON |

| Hand-rolled `json_escape` / `json_get_*` call sites | RapidJSON Writer / Document |

| `src/legacy/xml/**` + `src/base/BUILD.gn` | Remove TinyXML from base DLL |

| Living specs listed in design | Amend contradictory non-goals |



---



### Task 1: Third-party pins + GN



**Files:** manifest, `third_party/{rapidjson,pugixml,protobuf}/BUILD.gn`, `third_party/BUILD.gn`, `third_party/gn/BUILD.gn`, `smartgis.gni`



- [x] Add packages (GitHub fallbacks; Gitea when mirrored).

- [x] Header-only RapidJSON; pugixml source_set; protobuf group (cmake install when ready).

- [x] `build.bat t` / fetch packages needed for compile.



### Task 2: Style + tile JSON → RapidJSON



**Files:** `src/gis/style/**`, `src/gis/tile/provider/style_source.cc`, delete `detail/json_mini.*`



- [x] Rewrite parsers/emitters on `rapidjson::Document` / `Value` / `Writer`.

- [x] Update `BUILD.gn` deps to `//third_party:rapidjson`.

- [x] `style_test` green.



### Task 3: sdbd + plugin + catalog JSON → RapidJSON



**Files:** `sdbd/codec/sdbd_json.*`, `plugin/runtime/host/manifest/manifest.cc`, catalog/commands `json_escape` sites, dem/proj helpers, `json_append` / chrome_trace if still homemade encode.



- [x] Replace homemade trees and escapes with RapidJSON.

- [x] Related unit tests green.



### Task 4: Remove TinyXML; wire pugixml



**Files:** `src/legacy/xml/**`, `src/base/BUILD.gn`



- [x] Drop `xml_sources` from `base` DLL (no external TiXml callers).

- [x] Delete or stub legacy TinyXML sources; document pugixml as the XML dependency.

- [x] Keep `//third_party:pugixml` available for future XML TUs.



### Task 5: Docs + verify



- [x] Amend style / tile / IPC living notes per design.

- [x] Update `docs/superpowers/README.md` Active row.

- [x] `build.bat` (touched targets) green.



**最后更新:** 2026-09-28

