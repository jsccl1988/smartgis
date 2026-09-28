<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: archived checklist** (2026-09-28 merge B). Open work continues on the living umbrella in docs/superpowers/specs/ (see Active table). Do not reopen this as a hot twin.


# `src/legacy/app` subdirectory + SP3 strangler — Implementation Plan


> **Design living:** SP decisions live in [../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md). This file is the checklist only.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Execute on **master**. Do **not** `git commit` unless the user asks. Do **not** create branches. Do **not** run `build.bat` / `ninja` / any `out/*.exe` (human verifies). Spec: [`../specs/2026-09-27-legacy-app-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-app-subdirectory-layout-design.md).

**Partial land (2026-09-27):** Layout + break includes + `content` map_bootstrap + as-built docs landed. Deferred: `draft_commit` extract (`TODO(sp3)` — `MapScene` draft/Feature still scene-local), full thin-view strangler into `app/views`, and all human compile / test verification steps.

**Goal:** Split flat `src/legacy/app/` into `core` / `shell` / `doc` / `view` (scheme C break includes) while extracting HWND-free host bootstrap and draft-commit seams into `content`, leaving a thin MFC shell; freeze `:app` / `:app_core` / `dll_stem=app_core` / `SmartGis.exe`.

**Architecture:** Approach **C** interleaved — each module move is followed immediately by extract of migratable logic from that module. Landing: non-UI → `content/public`; presentable → `src/app/views`; leftover keeps MFC message maps and dock chrome only.

**Tech Stack:** C++23, GN/`build.bat`, MFC Feature Pack (leftover only), existing `content_*_test` / Views self-test patterns.

## Global Constraints

- Work on **master** only; no feature branches; no commit unless user asks
- No Qt; no new endgame `#include "legacy/…"`
- Nesting cap `src/legacy/app/<module>/`
- Root freeze: `smart_gis.rc`, `resource.h`, `stdafx.*`, `res/`
- GN freeze: `:app`, `:app_core`, `dll_stem=app_core`, `output_name = "SmartGis"`, `:legacy_app_all`
- English comments on touched product/new code; copyright year 2026
- Agent does **not** compile or run exe/e2e/UI — print commands for the human
- Cross-link / do not contradict [`2026-09-14-app-legacy-split-design.md`](../specs/2026-09-14-app-legacy-split-design.md) or [`2026-09-19-legacy-host-behavior-extract-design.md`](../specs/2026-09-19-legacy-host-behavior-extract-design.md)

## File map (end state)

| Path | Responsibility |
| --- | --- |
| `src/legacy/app/BUILD.gn` | Frozen labels; sources under modules + root rc/stdafx |
| `src/legacy/app/core/smtapp.*` | Thin `app::SmtApp`; calls `content` bootstrap |
| `src/legacy/app/shell/*` | `CSmartGisApp`, `CMainFrame`, `CChildFrame` |
| `src/legacy/app/doc/*` | `CSmartGisDoc` |
| `src/legacy/app/view/*` | Thin MFC `CView` adapters |
| `src/content/public/map_bootstrap.h` (+ `.cc` / `*_test.cc`) | HWND-free sample/china map open + NewMap append policy |
| `src/content/public/draft_commit.h` (+ `.cc` / `*_test.cc`) | HWND-free draft→feature commit helper |
| `src/app/views/map_scene.*` | Viewport transform + call `content` draft helper |
| `src/legacy/app/README.md`, `docs/build/src-layout.md` | As-built module row |

---

## Progress note (2026-09-27)

**Landed:** physical `core/` / `shell/` / `doc/` / `view/` + root `stdafx`/`rc`/`res/`; scheme C includes + `BUILD.gn` source paths; `content` map_bootstrap (public header + impl + test target) with `SmtApp` delegating sample path resolution; `src/legacy/app/README.md` / `docs/build/src-layout.md` / related spec cross-links. **Open:** Task 5 `draft_commit` (deferred — `TODO(sp3)`); Task 6 full thin-view strangler; all human compile / `te` steps. Spec Status remains `accepted` (design locked; plan not fully complete).

---

## Phase P0 — Inventory (read-only)

### Task 1: Freeze call sites and include blast radius

**Files:**
- Read: `src/legacy/app/**`, `src/legacy/app/BUILD.gn`
- Search (CBM `search_code` / scoped): `legacy/app/smtapp`, `legacy/app/main_frame`, `legacy/app/smart_`

- [x] **Step 1: List every in-tree `#include "legacy/app/…"`** — confirm module paths; flag any stale flat includes

Expected: TUs under `src/legacy/app/` use `"legacy/app/<module>/…"`. Product endgame should show **zero** `legacy/app` includes (confirm; do not add any).

- [x] **Step 2: Confirm frozen deploy names**

`dll_stem = "app_core"`, `output_name = "SmartGis"`, `precompiled_header = "legacy/app/stdafx.h"`.

- [x] **Step 3: Do not commit**

---

## Phase P1 — Layout `core` + extract map bootstrap (interleaved)

### Task 2: Move `smtapp` into `core/` and break includes

**Files:**
- Create dirs: `src/legacy/app/core/`
- Move: `smtapp.h`, `smtapp.cpp` → `core/`
- Modify: every `#include "legacy/app/smtapp.h"` → `"legacy/app/core/smtapp.h"`
- Modify: `src/legacy/app/BUILD.gn` (`app_core` sources path)

**Interfaces:**
- Consumes: existing `app::SmtApp` API
- Produces: include path `legacy/app/core/smtapp.h` only

- [x] **Step 1: `git mv` sources** (skip if `core/smtapp.*` already present)

```bat
git mv src/legacy/app/smtapp.h src/legacy/app/core/smtapp.h
git mv src/legacy/app/smtapp.cpp src/legacy/app/core/smtapp.cpp
```

- [x] **Step 2: Rewrite includes**

```cpp
#include "legacy/app/core/smtapp.h"
```

- [x] **Step 3: Point `app_core` sources at `core/smtapp.cpp`**

```gn
smt_shared_library("app_core") {
  dll_stem = "app_core"
  defines = [ "APP_CORE_EXPORTS" ]
  sources = [ "core/smtapp.cpp" ]
  # deps unchanged for this step
}
```

- [ ] **Step 4: Human compile check (agent prints only)**

```bat
.\build.bat legacy_app
```

Expected: `SmartGis.exe` + `app_core_d.dll` (or release stems) link.

- [x] **Step 5: Do not commit unless user asks**

---

### Task 3: Extract HWND-free map bootstrap into `content` (TDD)

**Files:**
- Create: `src/content/public/map_bootstrap.h`
- Create: `src/content/map_bootstrap.cc`
- Create: `src/content/map_bootstrap_test.cc`
- Modify: `src/content/BUILD.gn` (sources + `test("content_map_bootstrap_test")`)
- Modify: `src/legacy/app/core/smtapp.cpp` — delete free helpers; call `content::`

**Interfaces:**
- Consumes: filesystem paths under `out/` / app dir; GDAL/`DataSourceMgr` as today (keep types the helper already needs; prefer opaque path strings + callback or existing mgr pointers already used by leftover)
- Produces (names locked for later tasks):

```cpp
// content/public/map_bootstrap.h
namespace content {

struct SampleMapOpenResult {
  bool opened = false;
  int layer_count = 0;
  long long feature_count = 0;
};

// Resolve china_city.gpkg / geojson candidates under |search_roots|.
// Pure path policy — no HWND.
std::vector<std::string> resolve_sample_map_candidates(
    const std::vector<std::string>& search_roots);

// Open first existing candidate; fill |out_path| with chosen file.
// Returns false if none exist.
bool try_resolve_existing_sample_map(
    const std::vector<std::string>& search_roots,
    std::string* out_path);

}  // namespace content
```

Lift the body of `resolve_sample_geojson` / open policy from `smtapp.cpp` into these helpers. Keep `SmtApp::InitSmtMap` / `InitSmtDataSource` as orchestrators that still call `SmtMapMgr` / `DataSourceMgr` but use `content::` for path selection.

- [x] **Step 1: Write failing unit test**

```cpp
// map_bootstrap_test.cc — sketch
TEST(MapBootstrapTest, ResolvePrefersGpkgWhenListedFirst) {
  // Use a temp dir with only a .gpkg touch file or fixture path from testing/data.
  std::vector<std::string> roots = { /* fixture dir */ };
  auto cands = content::resolve_sample_map_candidates(roots);
  ASSERT_FALSE(cands.empty());
  EXPECT_NE(cands.front().find("china_city"), std::string::npos);
}
```

- [ ] **Step 2: Human runs test (expect FAIL before impl)**

```bat
.\build.bat te content_map_bootstrap_test
```

- [x] **Step 3: Implement `map_bootstrap` + wire GN**

- [x] **Step 4: Thin `SmtApp` — call helpers; remove duplicated path logic from `core/smtapp.cpp`**

- [ ] **Step 5: Human re-run**

```bat
.\build.bat te content_map_bootstrap_test
.\build.bat legacy_app
```

Expected: test PASS; legacy_app still links.

- [x] **Step 6: Do not commit unless user asks**

---

## Phase P2 — Layout `shell` / `doc` / `view` (break includes)

### Task 4: Move shell + doc + view; rewrite all includes

**Files:**
- Move into `shell/`: `smart_gis.*`, `main_frame.*`, `child_frame.*`
- Move into `doc/`: `smart_gis_doc.*`
- Move into `view/`: `smart_gis_view.*`, `smart_map_edit_view.*`, `smart_data_source_view.*`, `smart_3d_view.*`
- Keep at root: `stdafx.*`, `resource.h`, `smart_gis.rc`, `res/`
- Modify: `BUILD.gn` `:app` sources list
- Modify: internal includes among moved headers; RC / `stdafx` if they name headers

**Interfaces:**
- Produces include map from spec §5.1

- [x] **Step 1: `git mv` into module dirs**

```bat
mkdir src\legacy\app\shell src\legacy\app\doc src\legacy\app\view 2>nul
git mv src/legacy/app/smart_gis.h src/legacy/app/shell/smart_gis.h
git mv src/legacy/app/smart_gis.cpp src/legacy/app/shell/smart_gis.cpp
git mv src/legacy/app/main_frame.h src/legacy/app/shell/main_frame.h
git mv src/legacy/app/main_frame.cpp src/legacy/app/shell/main_frame.cpp
git mv src/legacy/app/child_frame.h src/legacy/app/shell/child_frame.h
git mv src/legacy/app/child_frame.cpp src/legacy/app/shell/child_frame.cpp
git mv src/legacy/app/smart_gis_doc.h src/legacy/app/doc/smart_gis_doc.h
git mv src/legacy/app/smart_gis_doc.cpp src/legacy/app/doc/smart_gis_doc.cpp
git mv src/legacy/app/smart_gis_view.h src/legacy/app/view/smart_gis_view.h
git mv src/legacy/app/smart_gis_view.cpp src/legacy/app/view/smart_gis_view.cpp
git mv src/legacy/app/smart_map_edit_view.h src/legacy/app/view/smart_map_edit_view.h
git mv src/legacy/app/smart_map_edit_view.cpp src/legacy/app/view/smart_map_edit_view.cpp
git mv src/legacy/app/smart_data_source_view.h src/legacy/app/view/smart_data_source_view.h
git mv src/legacy/app/smart_data_source_view.cpp src/legacy/app/view/smart_data_source_view.cpp
git mv src/legacy/app/smart_3d_view.h src/legacy/app/view/smart_3d_view.h
git mv src/legacy/app/smart_3d_view.cpp src/legacy/app/view/smart_3d_view.cpp
```

- [x] **Step 2: Rewrite every moved include to `legacy/app/<module>/…`**

No shim at old paths. Example:

```cpp
#include "legacy/app/shell/main_frame.h"
#include "legacy/app/doc/smart_gis_doc.h"
#include "legacy/app/view/smart_map_edit_view.h"
#include "legacy/app/core/smtapp.h"
#include "legacy/app/stdafx.h"
```

- [x] **Step 3: Update `:app` sources in `BUILD.gn`**

```gn
smt_mfc_executable("app") {
  output_name = "SmartGis"
  precompiled_header = "legacy/app/stdafx.h"
  sources = [
    "doc/smart_gis_doc.cpp",
    "shell/child_frame.cpp",
    "shell/main_frame.cpp",
    "shell/smart_gis.cpp",
    "view/smart_3d_view.cpp",
    "view/smart_data_source_view.cpp",
    "view/smart_gis_view.cpp",
    "view/smart_map_edit_view.cpp",
    "smart_gis.rc",
    "stdafx.cpp",
  ]
  deps = [
    ":app_core",
    # … existing deps unchanged …
  ]
}
```

(Working tree may already match; confirm `doc/smart_gis_doc.cpp` is listed — not under `view/`.)

- [x] **Step 4: Scoped search — zero stale includes**

Search pattern: `#include "legacy/app/smtapp.h"` / `main_frame.h` / `smart_gis_view.h` without module segment. Only archive docs may keep old paths.

- [ ] **Step 5: Human compile**

```bat
.\build.bat legacy_app
```

- [x] **Step 6: Do not commit unless user asks**

---

## Phase P3 — SP3 draft-commit seam + thin views (interleaved)

### Task 5: Extract HWND-free draft commit helper (TDD)

> **Deferred:** not landed. `TODO(sp3)` remains; `MapScene` draft / Feature commit stays scene-local. Leave all steps unchecked until a follow-up extract.

**Files:**
- Create: `src/content/public/draft_commit.h`
- Create: `src/content/draft_commit.cc`
- Create: `src/content/draft_commit_test.cc`
- Modify: `src/content/BUILD.gn`
- Modify: `src/app/views/map_scene.cc` (`append_from_draft` delegates after transform)
- Modify: `src/legacy/app/view/smart_map_edit_view.cpp` only if it duplicates commit logic (thin comment + call leftover/xview that already goes through product path — do not pull Views into MFC)

**Interfaces:**

```cpp
// content/public/draft_commit.h
namespace content {

struct DraftVertex {
  double x = 0;
  double y = 0;
};

struct DraftCommitInput {
  // Map-space vertices (already transformed off the viewport).
  std::vector<DraftVertex> vertices;
  // Opaque layer / geometry kind ids already used by MapScene — keep as
  // existing product types or string/int tags; do not take HWND/HDC.
};

struct DraftCommitResult {
  bool committed = false;
  int feature_count = 0;
};

// Apply draft vertices into the active edit session / map model.
// No Win32 types. MapScene performs view→map transform then calls this.
DraftCommitResult commit_draft_features(DraftCommitInput input /*, session refs */);

}  // namespace content
```

Implement against the same `EditSession` / map append path `MapScene::append_from_draft` already uses — move the **non-viewport** body only.

- [ ] **Step 1: Write failing `content_draft_commit_test`** (empty vertices → `committed == false`; minimal synthetic vertices → accept/reject per current MapScene rules)

- [ ] **Step 2: Human run expect FAIL**

```bat
.\build.bat te content_draft_commit_test
```

- [ ] **Step 3: Implement helper; delegate from `MapScene::append_from_draft`**

Keep pan/scale / rubber-band math in `MapScene`; pass map-space vertices into `content::commit_draft_features`.

- [ ] **Step 4: Human run**

```bat
.\build.bat te content_draft_commit_test
```

Expected: PASS. (Views `--self-test` optional human follow-up; agent must not run.)

- [ ] **Step 5: Do not commit unless user asks**

---

### Task 6: Thin MFC `view/` adapters + English intent comments

> **Deferred:** full thin-view strangler of all view behavior into `app/views` not landed. Layout `view/` dirs exist; behavioral extract remains open.

**Files:**
- Modify: `src/legacy/app/view/*.cpp` / `.h` (touched only)
- Optional comment pointers on `shell/main_frame.*` toward Views Catalog / AMBox

- [ ] **Step 1: For each MFC view, ensure no duplicated attribute/catalog/bootstrap logic** — call leftover xview / `SmtApp` / content helpers; delete dead copies if found

- [ ] **Step 2: Add short English class-intent comments on touched `class` definitions** (per comments.mdc)

- [ ] **Step 3: Confirm `src/app/views` still owns presentable Catalog / Attribute / Scene3d** — no new dependency from Views → `legacy/app`

- [ ] **Step 4: Human optional**

```bat
.\build.bat legacy_app
```

- [ ] **Step 5: Do not commit unless user asks**

---

## Phase P4 — Docs + SP3 cross-links

### Task 7: As-built docs and living-spec pointers

**Files:**
- Modify: `src/legacy/app/README.md` — module tree + include map + SP3 extract pointers
- Modify: `docs/build/src-layout.md` — App (leftover) row mentions `core/shell/doc/view`
- Modify: `docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md` — deferred draft/bootstrap → this plan/spec
- Modify: `docs/superpowers/specs/2026-09-14-app-legacy-split-design.md` — Related link to this subdirectory spec
- Modify: `docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md` — child table note for layout vehicle
- Modify: `docs/README.md` — index rows if missing

- [x] **Step 1: Rewrite `src/legacy/app/README.md` module table** (root freeze + include prefix `legacy/app/<module>/`)

- [x] **Step 2: One-line `src-layout` App leftover update**

- [x] **Step 3: SP3 / app-legacy-split / SP0 cross-links only — no contradictory Status**

- [x] **Step 4: Mark this plan tasks complete as work lands; leave human build boxes unchecked until verified**

- [x] **Step 5: Do not commit unless user asks**

---

## Include map (locked)

| Old | New |
| --- | --- |
| `legacy/app/smtapp.h` | `legacy/app/core/smtapp.h` |
| `legacy/app/smart_gis.h` | `legacy/app/shell/smart_gis.h` |
| `legacy/app/main_frame.h` | `legacy/app/shell/main_frame.h` |
| `legacy/app/child_frame.h` | `legacy/app/shell/child_frame.h` |
| `legacy/app/smart_gis_doc.h` | `legacy/app/doc/smart_gis_doc.h` |
| `legacy/app/smart_gis_view.h` | `legacy/app/view/smart_gis_view.h` |
| `legacy/app/smart_map_edit_view.h` | `legacy/app/view/smart_map_edit_view.h` |
| `legacy/app/smart_data_source_view.h` | `legacy/app/view/smart_data_source_view.h` |
| `legacy/app/smart_3d_view.h` | `legacy/app/view/smart_3d_view.h` |
| `legacy/app/stdafx.h` | unchanged |
| `legacy/app/resource.h` | unchanged |

---

## Plan self-review

| Spec requirement | Task |
| --- | --- |
| Scheme C break includes | Tasks 2, 4 |
| Interleaved extract | Tasks 3 (after core move), 5–6 (after view move) |
| Root rc/stdafx/res freeze | Tasks 4, 7 |
| GN label / stem freeze | Tasks 2–4 |
| Landing content + app/views | Tasks 3, 5, 6 |
| Cross-link SP3 / app-legacy-split | Task 7 |
| No agent compile | All human-run steps |
