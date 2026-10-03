<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` subdirectory layout + SP3 strangler (scheme C)


> **Status: superseded** (2026-09-28 merge). Merged into umbrella §SP3 (legacy/app layout). Do not revise here except mechanical link fixes.

**Date:** 2026-09-27  
**Status:** accepted  
**Scope:** Physical scheme **C** (headers move with dirs; break in-tree includes) **interleaved** with **SP3** behavioral extract — pull migratable host logic out of the flat MFC package so `legacy/app` becomes a thin MFC shell.  
**Plan:** [`../plans/2026-09-27-legacy-app-subdirectory-layout.md`](../plans/2026-09-27-legacy-app-subdirectory-layout.md)  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| App / UI leftover physical split | [`2026-09-14-app-legacy-split-design.md`](2026-09-14-app-legacy-split-design.md) | **accepted** — Phase 1+2 landed; this spec **refines inside** `legacy/app` and deepens SP3 extract; does not reopen stop-compile gate |
| SP3 host behavior (Attribute / Catalog) | [`2026-09-19-legacy-host-behavior-extract-design.md`](2026-09-19-legacy-host-behavior-extract-design.md) | **active** — Attribute + Catalog snapshot **done**; this workstream **continues** deferred SP3 (bootstrap, draft commit seam, thin MFC views) |
| SP0 umbrella | [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) | SP3 path ownership; do not contradict dependency direction |
| Peer layout (scheme C break includes) | [`2026-09-27-tool-subdirectory-layout-design.md`](2026-09-27-tool-subdirectory-layout-design.md) | Same idea: directory = responsibility; no shim umbrellas |
| Peer layout (leftover dual-run) | [`2026-09-27-legacy-render-subdirectory-dual-run-design.md`](2026-09-27-legacy-render-subdirectory-dual-run-design.md) | Leftover stays opt-in strangler; Views remains endgame |
| Atmosphere / GPU layout peers | [`2026-09-27-atmosphere-subdirectory-layout-design.md`](2026-09-27-atmosphere-subdirectory-layout-design.md), [`2026-09-27-gpu-subdirectory-layout-design.md`](2026-09-27-gpu-subdirectory-layout-design.md) | Responsibility dirs + stable aggregate labels |
| China sample bootstrap | [`2026-09-18-china-city-map-plpt-design.md`](2026-09-18-china-city-map-plpt-design.md) | Sample paths / gpkg layers stay; ownership moves to HWND-free helpers |
| Product as-built | [`../../build/src-layout.md`](../../build/src-layout.md), [`../../../src/legacy/app/README.md`](../../../src/legacy/app/README.md) | Update when landing |

---

## 1. Goal / Non-goals

### 1.1 Goal

1. **Split the flat dump** under `src/legacy/app/` into responsibility modules (`core` / `shell` / `doc` / `view`) under nesting cap `src/legacy/app/<module>/`.
2. **Break includes** (tool-style scheme C): every in-tree `#include "legacy/app/<file>.h"` that moves becomes `#include "legacy/app/<module>/<file>.h"` in the **same** change. No root umbrella / shim headers for moved types.
3. **Interleave SP3 extract:** as files move, extract HWND-free / presentable behavior into `content` (or existing equivalent) and `src/app/views`; leave only MFC message maps, document templates, dock chrome, and thin adapters in `legacy/app`.
4. **Freeze deploy / GN public labels:** `//src/legacy/app:app` → `SmartGis.exe`; `//src/legacy/app:app_core` with `dll_stem=app_core`; `group("legacy_app_all")` unchanged for `build.bat legacy_app`.

### 1.2 Non-goals

- **Do not** change `dll_stem=app_core`, exe name `SmartGis`, or opt-in gate `smt_build_app` / `build.bat legacy_app`.
- **Do not** move `.rc` / `resource.h` / `stdafx.*` off the package root; **do not** relocate `res/`.
- **Do not** revive default compile of MFC in everyday `build.bat app` (Views remains default).
- **Do not** introduce Qt, Feature Pack as endgame, dock/MDI as destination chrome, or new product features inside leftover.
- **Do not** deepen past `src/legacy/app/<module>/…` (no `view/edit/stroke/…` public trees).
- **Do not** swallow SP1 (tool Workspace), SP2 (present/paint HWND), or SP4 (scene3d → World/GpuScene) ownership — only call their existing seams.
- **Do not** force a third public C++ namespace; leftover keeps existing `app::SmtApp` / MFC class names; new HWND-free APIs stay `content` / `app` two-layer + `snake_case`.

---

## 2. Inventory (2026-09-27)

Flat package today (CBM + tree):

| File | Role |
| --- | --- |
| `smtapp.h` / `smtapp.cpp` | `app::SmtApp` in `app_core` DLL — Init / DelayInit / map+DS+style bootstrap |
| `smart_gis.h` / `.cpp` | `CSmartGisApp` — `InitInstance`, document templates, `--self-test`, deferred EDIT open |
| `main_frame.*` / `child_frame.*` | MDI frame + catalog / AMBox dock chrome |
| `smart_gis_doc.*` | `CSmartGisDoc` |
| `smart_gis_view.*` / `smart_map_edit_view.*` / `smart_data_source_view.*` / `smart_3d_view.*` | MFC `CView` hosts |
| `smart_gis.rc` / `resource.h` / `stdafx.*` / `res/` | Resources + PCH — **stay at root** |

**GN (frozen labels):**

```
//src/legacy/app:app_core   smt_shared_library  dll_stem=app_core  sources=[smtapp.cpp]
//src/legacy/app:app        smt_mfc_executable   output_name=SmartGis
//src/legacy/app:legacy_app_all  group → :app
```

**Already extracted (SP3 wave 1 — do not re-litigate):**

- `content::feature_attrs` + `MapScene` attribute writeback  
- `content::LayerDesc` + `layers_to_catalog_json`  

**Still in leftover / Views seams (this workstream):**

| Hotspot | Today | Target landing |
| --- | --- | --- |
| Sample / china map open helpers in `smtapp.cpp` (`resolve_sample_geojson`, `open_or_create_sample_geojson_ds`, parts of `InitSmtMap` / `InitSmtDataSource`) | HWND-free but trapped in `app_core` | **`content`** (public header + `.cc` + unit test) |
| `InitStyleMgr` / listener / aux module load | Mostly leftover singletons | Thin keep in `core/`; extract only pure path/policy helpers if clearly HWND-free |
| Edit draft commit | `MapScene::append_from_draft` (viewport-bound) | Split: HWND-free commit helper → **`content`**; viewport transform stays **`content::MapScene`** |
| MFC view paint / SetOperMap / framing | Inside `smart_*_view` + xview | Presentable policy already on Views path; MFC views become **thin** adapters (no new GDI engine) |
| CatalogCall pipe | Stub beyond snapshot JSON | Optional thin progress only; full delta pipe stays later |

---

## 3. Approaches considered

| Approach | Idea | Pros | Cons |
| --- | --- | --- | --- |
| **A. Docs only** | Keep flat tree; only update SP3 text | Zero churn | Flat dump stays; extract keeps fighting paths |
| **B. Layout first, extract later** | Scheme C move, then separate SP3 PR | Clean review of moves | Two landings; behavior stays duplicated longer |
| **C. Interleaved layout + extract** (**locked**) | `git mv` into modules **and** extract migratable logic in the same effort / plan | Matches user scope; leaves thin shell sooner | Larger plan; needs path ownership discipline |

**Locked: Approach C.** Layout tasks and extract tasks alternate so each module lands thinner than it started — not a pure mechanical move of fat TUs.

---

## 4. Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Scheme C** inside `src/legacy/app`: headers move with dirs; **break** includes; **no** shim headers at old paths. |
| 2 | **Interleaved SP3:** extract as thoroughly as practical in this workstream (bootstrap + draft-commit seam + thin views). |
| 3 | **Landing layered:** non-UI → `src/content` (`content/public/…`); presentable host composition → `src/app/views`; leftover keeps MFC shell only. |
| 4 | **Root freeze:** `smart_gis.rc`, `resource.h`, `stdafx.h`, `stdafx.cpp`, `res/` stay at `src/legacy/app/`. |
| 5 | **GN freeze:** labels `:app`, `:app_core`, `dll_stem=app_core`, `output_name = "SmartGis"`, `:legacy_app_all`. Fine-grained `source_set`s allowed under modules if root still exposes the same public labels. |
| 6 | **PCH:** `precompiled_header = "legacy/app/stdafx.h"` stays (root path). |
| 7 | Nesting cap: `src/legacy/app/<module>/…` only. |
| 8 | Endgame must not gain new `#include "legacy/…"`. Legacy → `content/public` unidirectional OK. |
| 9 | Git: **master** only; no feature branch; commit only when user asks. |
| 10 | Agent does **not** run `build.bat` / ninja / exe; human verifies `build.bat legacy_app` (+ named tests). |

---

## 5. Target tree

```
src/legacy/app/
  BUILD.gn                 # :app_core :app :legacy_app_all (labels frozen)
  README.md
  resource.h               # root
  smart_gis.rc             # root
  stdafx.h  stdafx.cpp     # root PCH
  res/                     # icons / cursors / .rc2 — stays

  core/                    # app_core DLL
    smtapp.h
    smtapp.cpp             # thin: Init* call content helpers + leftover singletons

  shell/                   # MFC app + frames
    smart_gis.h  smart_gis.cpp
    main_frame.h main_frame.cpp
    child_frame.h child_frame.cpp

  doc/
    smart_gis_doc.h  smart_gis_doc.cpp

  view/                    # MFC CView subclasses (thin adapters after extract)
    smart_gis_view.h  smart_gis_view.cpp
    smart_map_edit_view.h  smart_map_edit_view.cpp
    smart_data_source_view.h  smart_data_source_view.cpp
    smart_3d_view.h  smart_3d_view.cpp
```

**Forbidden after land:** public headers for moved types at `src/legacy/app/*.h` (except `stdafx.h` / `resource.h`). No `legacy/app/smtapp.h` shim that only includes `core/smtapp.h`.

### 5.1 Include migration map

| Old | New |
| --- | --- |
| `legacy/app/smtapp.h` | `legacy/app/core/bootstrap.h` |
| `legacy/app/smart_gis.h` | `legacy/app/shell/frame/win_app.h` |
| `legacy/app/main_frame.h` | `legacy/app/shell/frame/main_frame.h` |
| `legacy/app/child_frame.h` | `legacy/app/shell/frame/child_frame.h` |
| `legacy/app/smart_gis_doc.h` | `legacy/app/doc/document.h` |
| `legacy/app/smart_gis_view.h` | `legacy/app/view/map/map.h` |
| `legacy/app/smart_map_edit_view.h` | `legacy/app/view/edit_view.h` |
| `legacy/app/smart_data_source_view.h` | `legacy/app/view/data_view.h` |
| `legacy/app/smart_3d_view.h` | `legacy/app/view/scene3d_view.h` |
| `legacy/app/stdafx.h` | **unchanged** (root) |
| `legacy/app/resource.h` | **unchanged** (root) |

RC / `stdafx` includes of moved headers must use the new paths. In-tree callers (mostly self + docs) rewrite in the same change; scoped search for `"legacy/app/` at land time.

### 5.2 Extract landing map (layered)

| Concern | Extract to | Keep in leftover |
| --- | --- | --- |
| Sample GeoJSON / `china_city.gpkg` resolve + open policy | `content/public/…` + `content/*.cc` + `*_test.cc` | `SmtApp` calls helper; MFC message pump order |
| Map NewMap + AppendLayer bootstrap sequence (HWND-free) | `content` helper used by `SmtApp::InitSmtMap` **and** optionally Views open path | MFC catalog dock refresh triggers |
| Feature token / attrs / LayerDesc JSON | **Already** in `content` (SP3 wave 1) | — |
| Draft → features commit without HDC | `content` (new) + `MapScene` keeps view→map transform | MFC edit view message handlers |
| Status bar strings / ribbon / dock create | — | `shell/main_frame` |
| Document template / `InitInstance` ordering | — | `shell/smart_gis` (may call content bootstrap) |
| 3D HWND create / leftover GL lift | Thin `view/smart_3d_view`; modern path stays `app/views` Scene3d | MFC menu `ID_WND_3D` |

---

## 6. GN shape

```
//src/legacy/app:app_core     smt_shared_library  sources under core/
//src/legacy/app:app          smt_mfc_executable  shell/ + doc/ + view/ + root rc/stdafx
//src/legacy/app:legacy_app_all  group → :app
```

Optional (allowed, not required in v1):

```
//src/legacy/app:shell_sources   source_set
//src/legacy/app:view_sources    source_set
//src/legacy/app:doc_sources     source_set
```

Callers outside the package continue to depend on `:app` / `:app_core` / `:legacy_app_all` only.

`app_core` may add a dep on `//src:content` (or the content source_set label already used by Views) for bootstrap helpers — **not** the reverse.

---

## 7. Dependency / layering

```
SmartGisViews.exe (src/app/views)     ← endgame; no new legacy includes
        → content/public
        → tool::Workspace / gis::EditSession
        → render / gis

SmartGis.exe (src/legacy/app thin shell)
        → legacy/app/core (app_core) → content helpers (new) + leftover singletons
        → legacy/ui / legacy/tool / legacy/render
```

**Forbidden:** `src/app/views` / `src/ui/views` / `src/content` gaining `#include "legacy/app/…"`.

---

## 8. Migration phases (plan implements)

| Phase | Work | Gate |
| --- | --- | --- |
| 0 | Spec accepted (this file) + plan written | User already approved Approach C |
| 1 | Create dirs; `git mv`; rewrite includes + PCH/RC refs | `rg` shows no stale `"legacy/app/smtapp.h"` etc. (except archive docs) |
| 2 | Rewire `BUILD.gn` paths; keep labels / stems | Human: `build.bat legacy_app` compiles |
| 3 | Extract sample/map bootstrap → `content` + test; thin `SmtApp` | Unit test green (human); MFC still calls helper |
| 4 | Extract draft-commit HWND-free seam; `MapScene` + MFC edit view thin | Views path behavior preserved; test covers helper |
| 5 | Thin remaining `view/` / comments; update README + `src-layout` + cross-links | Docs match tree; SP3 deferred boxes updated |

Phases 1–4 are **interleaved in the plan** (move a module → extract its free logic → next module), not a pure layout mega-diff followed by a second extract mega-diff.

---

## 9. Relation to living SP3 / app-legacy-split

- **`2026-09-14-app-legacy-split-design.md`:** remains the authority for *why* MFC lives under `legacy/app` and for stop-compile / Views default. This file owns **subdirectory + continued strangler inside that package**.
- **`2026-09-19-legacy-host-behavior-extract-design.md`:** wave-1 success criteria stay checked. Deferred items (edit draft commit; further host bootstrap) are **picked up here** — revise that spec’s deferred section to point at this file rather than inventing a parallel SP3 fork.
- **SP0 umbrella:** SP3 path ownership still `legacy/app` + `content` + `app/views`; this is the SP3+layout vehicle for 2026-09-27.

---

## 10. Done when

1. Target tree matches §5; root only holds BUILD/README/rc/resource/stdafx/res.
2. All in-tree includes use §5.1 paths; zero shims for moved headers.
3. `:app` / `:app_core` / `dll_stem` / `SmartGis.exe` unchanged.
4. HWND-free bootstrap (+ draft-commit seam as practical) live under `content` with unit tests; `SmtApp` / MFC views are thin callers.
5. No new endgame → legacy includes.
6. `src/legacy/app/README.md` + App leftover row in `docs/build/src-layout.md` describe modules; SP3 / app-legacy-split cross-links updated.
7. Human confirms `build.bat legacy_app` (and named content/views tests) green.

---

## 11. Spec self-review

| Check | Result |
| --- | --- |
| Placeholders | None. |
| Contradictions | Explicitly extends SP3; does not reopen Attribute/Catalog landing or stop-compile. |
| Scope | Layout C + interleaved extract; SP1/SP2/SP4 out. |
| Ambiguity | Module names locked (`core`/`shell`/`doc`/`view`); root freeze locked; CatalogCall full pipe still optional. |
| Nesting | Cap = one module under `legacy/app`. |
| Approach | **C** locked by user. |
