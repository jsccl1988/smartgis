# App legacy split + Views upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 MFC `SmartGis.exe` 与 MFC UI 迁到 `src/legacy_app` / `src/legacy_ui`；`src/app` 只留 views/winui；并行推进 Views 能力 1→5。

**Architecture:** 仿 `legacy_tool`/`legacy_render` 物理平移 + 无转发头 include 改名；终局壳不链 leftover。Phase 2 在 `ViewHost`/`ui::views` 上闭环，不改 leftover 业务。

**Tech Stack:** C++23, GN/`build.bat`, MFC leftover, `ui::views`, `content::ViewHost`, `tool::Workspace`

## Global Constraints

- Work on `master` only; do not create feature branches.
- Paths: `src/legacy_app/`, `src/legacy_ui/`; keep `dll_stem` `ui_legacy` and `app_core`.
- No forward headers from old `ui/gui` paths.
- No Qt; no true dock/MDI; Views widgets never hold `SmtFeature*`.
- Source comments English; user-facing docs Chinese.
- Copyright `(c) 2026 The Mogu Authors.` on touched/new files.
- New-tree functions `snake_case`; public namespaces ≤ two layers.
- **Do not `git commit` unless the user explicitly asks.**
- Spec: `docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`
- Partition: Phase 1 agents own `legacy_*` + include/GN rewrites; Phase 2 agents own `src/app/views`, `src/ui/views`, `src/tool`, `src/content`, `src/sdb/edit` — avoid overlapping writes.

## File map

| Path | Role |
| --- | --- |
| `src/legacy_app/**` | MFC exe + `app_core` |
| `src/legacy_ui/**` | MFC chrome → `ui_legacy` DLL |
| `src/app/views/**` | SmartGisViews composition |
| `src/app/winui/**` | unchanged location (prototype) |
| `src/ui/views/**` | endgame toolkit |
| `src/ui/BUILD.gn` | forward group to `legacy_ui` or thin views-only |
| `docs/build/src-layout.md` | layout table |
| `docs/build/ui-views-skia.md` | leftover paths |

---

### Task 1: Move MFC app → `src/legacy_app`

**Files:**
- Create: `src/legacy_app/` (git mv from `src/app` MFC roots + `app_core`)
- Modify: root `BUILD.gn` / `//:smartgis` / `build.bat` app target labels
- Modify: all `#include "app/…"` for MFC/`app_core` → `"legacy_app/…"`
- Create: `src/legacy_app/README.md`
- Modify: `src/app/BUILD.gn` — remove MFC executable; optional README-only or group

**Interfaces:**
- Produces: `//src/legacy_app:app` (`output_name = "SmartGis"`), `//src/legacy_app/app_core:app_core`
- Consumes: existing MFC sources unchanged in logic

- [x] **Step 1: Inventory move set**

Move (do not leave copies):

- `main_frame.*`, `child_frame.*`, `smart_*.*`, `stdafx.*`, `resource.h`, `smart_gis.rc`, `res/`, `ReadMe.txt`, `SmartGis.aps` (if tracked)
- `app_core/` entire tree

Keep in `src/app/`: `views/`, `winui/` only (+ new README).

- [x] **Step 2: git mv + fix BUILD.gn**
```gn
# src/legacy_app/BUILD.gn — adapt from former src/app/BUILD.gn
smt_mfc_executable("app") {
  output_name = "SmartGis"
  precompiled_header = "legacy_app/stdafx.h"
  # sources: same basenames under legacy_app/
  deps = [
    "//src/legacy_app/app_core:app_core",
    # … legacy_ui labels after Task 2, or //src/ui:* forwarders meanwhile
  ]
}
```

Update `app_core` includes to `"legacy_app/app_core/app_smtapp.h"`.  
PCH / `#include "app/stdafx.h"` → `"legacy_app/stdafx.h"`.

- [x] **Step 3: Rewrite call-site includes / GN**

Scoped replace (repo product trees, not `third_party/`):

| From | To |
| --- | --- |
| `#include "app/stdafx.h"` | `#include "legacy_app/stdafx.h"` |
| `#include "app/app_core/…"` | `#include "legacy_app/app_core/…"` |
| `#include "app/main_frame.h"` etc. | `#include "legacy_app/…"` |
| `//src/legacy_app:app` | `//src/legacy_app:app` |
| `//src/legacy_app/app_core:app_core` | `//src/legacy_app/app_core:app_core` |

Do **not** rewrite `src/app/views` or `src/app/winui` includes that correctly say `app/views/…`.

- [x] **Step 4: Smoke build**

Run: `build.bat app` (or equivalent ninja `smartgis` / `SmartGis`)  
Expected: links or only compile errors fixable without reverting move.

- [x] **Step 5: Docs touch**

Update path rows in `docs/build/src-layout.md` App layer; add `src/legacy_app/README.md`.

---

### Task 2: Move MFC UI → `src/legacy_ui`

**Files:**
- Create: `src/legacy_ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` via git mv
- Modify: `src/legacy_ui/BUILD.gn` (from `src/ui/BUILD.gn` ui_legacy block)
- Modify: `src/ui/BUILD.gn` — keep `views`; `group("ui_legacy")` → `public_deps = [ "//src/legacy_ui:ui_legacy" ]`
- Modify: all `#include "ui/gui/…"` etc. → `"legacy_ui/gui/…"` (same for mfc_ex/xview/xcatalog/xambox/chart)
- Create: `src/legacy_ui/README.md`

**Interfaces:**
- Produces: `//src/legacy_ui:ui_legacy` (`dll_stem = "ui_legacy"`)
- Consumes: Task 1 deps may temporarily use `//src/ui:ui_legacy` forwarder

- [x] **Step 1: git mv six trees**

```
src/ui/gui → src/legacy_ui/gui
src/ui/mfc_ex → src/legacy_ui/mfc_ex
src/ui/xview → src/legacy_ui/xview
src/ui/xcatalog → src/legacy_ui/xcatalog
src/ui/xambox → src/legacy_ui/xambox
src/ui/chart → src/legacy_ui/chart
```

Leave `src/ui/views` in place.

- [x] **Step 2: GN aggregation**

Move `smt_shared_library("ui_legacy")` body to `src/legacy_ui/BUILD.gn`.  
Source deps become `//src/legacy_ui/gui:gui_sources` etc.  
Keep `group("gui")` style labels under each submodule pointing at `ui_legacy` DLL.

- [x] **Step 3: Include rewrite**

| From | To |
| --- | --- |
| `"ui/gui/` | `"legacy_ui/gui/` |
| `"ui/mfc_ex/` | `"legacy_ui/mfc_ex/` |
| `"ui/xview/` | `"legacy_ui/xview/` |
| `"ui/xcatalog/` | `"legacy_ui/xcatalog/` |
| `"ui/xambox/` | `"legacy_ui/xambox/` |
| `"ui/chart/` | `"legacy_ui/chart/` |

Do **not** rewrite `"ui/views/…"`.

Also fix `legacy_tool/group` comments/deps that mention `//src/ui:ui_legacy` (forwarder OK).

- [x] **Step 4: Smoke**

Run: `build.bat app` and confirm `ui_legacy_d.dll` (or release stem) still produced.  
Run: `build.bat views` — must not require `legacy_ui`.

- [x] **Step 5: Docs**

`src-layout.md` UI row; `ui-views-skia.md` leftover paths; migration spec leftover table → `legacy_ui`.

---

### Task 3: Phase 2.1 — Edit workflow on Views

**Files:**
- Modify: `src/app/views/browser_view.*`, `app_commands.*`
- Modify: `src/content/public/*` ViewHost / MapContents as needed
- Modify: `src/tool/*` workspace / gestures
- Test: `src/ui/views/views_unittests.cc` and/or `--self-test`

**Interfaces:**
- Consumes: `content::ViewHost::activate` / `execute` / `dispatch_input`, `tool::Workspace`, existing select/pan/identify
- Produces: user-visible edit draft + commit path from Views chrome (status bar feedback)

- [x] **Step 1:** Trace current MapViewport → ViewHost → Workspace path (CBM); list missing activate ids for draw/edit.
- [x] **Step 2:** Wire MenuBar / Ambox actions to edit commands; publish status on commit/cancel.
- [x] **Step 3:** Add/extend unittest or `--self-test` covering activate+execute without MFC.
- [x] **Step 4:** `build.bat views` green for this path.

---

### Task 4: Phase 2.2 — Writable AttributeTable

**Files:**
- Modify: `src/ui/views` AttributeTable (+ header)
- Modify: `src/app/views/browser_view.*` commit callback via opaque id
- Test: `views_unittests.cc`

**Interfaces:**
- Consumes: selection opaque ids from EventBus
- Produces: `AttributeTable` edit → callback `on_cell_commit(feature_token, field, value)` owned by BrowserView/ViewHost — **no** `SmtFeature*` in widget

- [x] **Step 1:** Failing test: set cell → callback fired with token/field/value.
- [x] **Step 2:** Implement minimal editable cell + commit on Enter/focus loss.
- [x] **Step 3:** BrowserView maps token → EditSession/feature write; errors → status bar.
- [x] **Step 4:** Build + test green.

---

### Task 5: Phase 2.3 — Catalog real layers

**Files:**
- Modify: `src/ui/views` CatalogView / LayerTree
- Modify: `src/app/views/browser_view.*` open-map → populate from MapContents/session
- Prefer existing `CatalogCall` JSON; no second legend protocol

**Interfaces:**
- Consumes: map open / layer list from content/sdb session APIs already used by Views
- Produces: LayerTree reflects visibility/active layer after open

- [x] **Step 1:** Replace demo-only populate when real layer list available.
- [x] **Step 2:** Toggle visibility → map refresh / status.
- [x] **Step 3:** Self-test or unittest with fake layer list.
- [x] **Step 4:** Build green (`views_unittests: ok`; `build.bat views`).

---

### Task 6: Phase 2.4 — Ambox / PluginHost enumeration

**Files:**
- Modify: `src/tool/command.*` (`for_each` if missing — see chrome-parity plan)
- Modify: `src/ui/views/ambox_view.*`
- Modify: `src/app/views/plugin_chrome.*` / `browser_view.*`

- [x] **Step 1:** Ensure `CommandCatalog::for_each` exists + tested.
- [x] **Step 2:** `AmboxView::populate_from_plugin_host` groups by id prefix.
- [x] **Step 3:** Click → ViewHost activate/execute; avoid leftover `CDlg*` on Views path.
- [x] **Step 4:** Build + unittest green.

---

### Task 7: Phase 2.5 — 3D tab HWND

**Files:**
- Modify: `src/ui/views` MapViewport attach for 3D / `OpenView::kScene3d`
- Modify: `src/app/views/browser_view.*` 3D tab

- [x] **Step 1:** Confirm 3D attach path; stabilize HWND create/destroy on tab switch.
- [x] **Step 2:** Basic pan/orbit input via ViewHost when device present; placeholder otherwise.
- [x] **Step 3:** `--self-test` does not crash with/without 3D device.
- [x] **Step 4:** Build green.

---

### Task 8: Stop-compile gate (after 1–5)

**Files:**
- Modify: `build.bat`, root `BUILD.gn`, spec Status

- [x] **Step 1:** Checklist 1–5 accepted (edit / AttributeTable / Catalog / Ambox / 3D); `--self-test` exit 0.
- [x] **Step 2:** Daily `build.bat app` → Views; MFC via `build.bat legacy_app` / `//:legacy_app_all` (`smt_build_app`).

---

## Parallel execution guide

| Track | Tasks | Owns paths |
| --- | --- | --- |
| A | 1 then 2 | `legacy_app`, `legacy_ui`, leftover include/GN, layout docs |
| B | 3 | `app/views`, `content`, `tool` (edit) |
| C | 4 | `ui/views` AttributeTable + browser_view writeback |
| D | 5 | Catalog/LayerTree + browser_view populate |

After A+B+C+D stable: Task 6 then 7 (may parallel if no file overlap).

## Spec coverage

| Spec section | Tasks |
| --- | --- |
| Phase 1 legacy_app | 1 |
| Phase 1 legacy_ui | 2 |
| Phase 2.1–2.5 | 3–7 |
| Stop-compile gate | 8 |
| Docs | 1, 2, 8 |

---

**最后更新：** 2026-09-14
