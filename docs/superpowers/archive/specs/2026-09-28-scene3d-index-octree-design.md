<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP4b — `scene3d` index layer + open-source octree (scheme A)


> **Status: superseded** (2026-09-28 merge). Merged into umbrella §SP4b. Do not revise here except mechanical link fixes.

**Status:** accepted  
**Date:** 2026-09-28  
**Scope:** Child of umbrella **SP4**. Inside `src/legacy/render/scene3d` only: drop the `bl3d_` file-name prefix, introduce an `index/` responsibility directory, and replace the hand-rolled octree implementation with a lightweight MIT header-only vendor behind a thin adapter. Keep exported types and DLL surface stable (`SmtScene`, `SmtSceneOctTree`, `LEGACY_RENDER_EXPORT`, `dll_stem = legacy_render`).  
**Relation to SP4:** Does **not** reopen or rewrite landed SP4 Success / Done-when checkboxes in [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md). SP4 already seeded AABB → `gis::World` and thinned DEM/map seams; this child only reshapes leftover scene indexing and file layout so the strangler stays maintainable. World / GpuScene migration remains SP4 / follow-on — not this workstream.  
**Plan:** [`../plans/2026-09-28-scene3d-index-octree.md`](../plans/2026-09-28-scene3d-index-octree.md)  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| SP0 umbrella | [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) | SP4 path ownership; this is **SP4b** child — do not widen ABI or reverse dependency direction |
| SP4 Scene3D → World / GpuScene | [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md) | **Parent** — landed Success stays; SP4b does not wholesale delete octree or move logic into World |
| DEM / host unify | [`2026-09-19-leftover-scene3d-dem-unify-design.md`](2026-09-19-leftover-scene3d-dem-unify-design.md) | `dem/` + bridge seeds stay; layout must not fight DemRaster authority |
| Dual-run / scene3d P2.4 layers | [`2026-09-27-legacy-render-subdirectory-dual-run-design.md`](2026-09-27-legacy-render-subdirectory-dual-run-design.md) | Extends P2.4 layers with `index/`; leftover stays opt-in strangler |
| Peer layout (scheme C, no shims) | [`2026-09-27-legacy-app-subdirectory-layout-design.md`](2026-09-27-legacy-app-subdirectory-layout-design.md) | Same include-break + colocation rules |
| Product as-built | [`../../build/src-layout.md`](../../build/src-layout.md), [`../../../src/legacy/render/scene3d/README.md`](../../../src/legacy/render/scene3d/README.md) | Update when landing |

---

## 1. Goal / Non-goals

### 1.1 Goal

1. **Scheme A only:** all work stays under `src/legacy/render/scene3d/**` (plus `third_party/` vendor pin and in-tree `#include` / `BUILD.gn` updates that name those paths).
2. **Drop `bl3d_` basenames** and spell spatial index files `octree` / `vertex_octree` (not `octtree`).
3. **Split index from scene shell:** pure spatial index lives in `index/` with **no** `LP3DRENDERDEVICE` / render-device dependency; `scene/` keeps thin `SmtScene` / `Smt3DObject` / vertex structs that may hold a device.
4. **Replace the bespoke octree body** with a lightweight **MIT header-only** library under `third_party/`, wrapped by an `index/` adapter that still exposes `SmtSceneOctTree` / `SmtSceneOctTreeNode` (and vertex-tree peers) to callers.
5. **Same-change migration:** rename + move + update `BUILD.gn` + every in-tree include (UI / tool / plugin / tests). Behavior and export names stay.

### 1.2 Non-goals

- **Do not** rewrite SP4 Success criteria, reopen World / GpuScene seeding policy, or wholesale replace `SmtScene` with `gis::World`.
- **Do not** pull PCL, OpenVDB, Embree, or any heavy spatial stack; **do not** use Assimp (or similar) as a scene/octree.
- **Do not** change `dll_stem` (`legacy_render`), `LEGACY_RENDER_EXPORT`, or rename exported `Smt*` types / LoadLibrary entry points.
- **Do not** add old-path shim headers (`bl3d_scene.h` forwarding to `scene.h`).
- **Do not** own SP2 present / bridge HWND paint, SP3 host extract, or deepen nesting past `legacy/render/scene3d/<module>/`.
- **Do not** introduce Qt; **do not** make `src/render` / `src/gis` gain new deps on `legacy/render`.
- **Do not** commit, branch, or run builds as part of authoring this spec (human owns compile).

---

## 2. Constraints

| Area | Rule |
| --- | --- |
| **ABI / exports** | Keep `SmtScene`, `SmtSceneOctTree`, `SmtSceneOctTreeNode`, `SmtVertexOctTree`, `SmtVertexOctTreeNode`, `Smt3DObject`, related structs, and `LEGACY_RENDER_EXPORT`. Method names that are existing leftover ABI may stay PascalCase until a later cleanup; **new** helpers in touched new-tree seams use `snake_case`. |
| **DLL** | Still compiled into `//src/legacy/render:legacy_render`. Public GN label for the scene3d aggregate stays usable (`scene3d_sources` / `group("scene3d")` as today). |
| **Naming (files)** | Strip `bl3d_`; use `octree` spelling; keep semantic prefixes `dem_*`, `map_*`, and bridge role names. Types remain `Smt*`. |
| **Colocation** | `.h` beside `.cpp`/`.cc` in the same module directory (`.cursor/rules/style/colocated-sources.mdc`). No headers-at-parent / `include/` split. |
| **Nesting** | Cap: `src/legacy/render/scene3d/<module>/…` with modules listed in §3. No deeper public trees. |
| **Device boundary** | `index/**` must not include or call `LP3DRENDERDEVICE` / `Smt*RenderDevice`. Update / Render / Select that need a device stay in `scene/` (or existing drawable modules) and query `index/` with AABB / object lists only. |
| **Dependency direction** | `legacy/render/scene3d` → vendor header + existing `gis` / `base/math` OK; endgame must not `#include "legacy/…"`. |
| **Git** | Work on **master**; no feature branch; commit only when the user asks. |

---

## 3. Target layout + rename map

### 3.1 Target tree

```
src/legacy/render/scene3d/
  BUILD.gn
  README.md
  index/          # octree / vertex_octree — no LP3DRENDERDEVICE
  scene/          # SmtScene, Smt3DObject, vertex3d — thin shell; may hold device
  primitive/
  feature/
  surface/
  dem/
  bridge/
  test/
```

`primitive/` `feature/` `surface/` `dem/` `bridge/` `test/` keep their current roles from dual-run P2.4; only `bl3d_*` scene/index files move or rename under this spec.

### 3.2 File rename map (disk today → target)

Inventory baseline is the tree on disk under `src/legacy/render/scene3d/` (CBM paths may still show pre-P2.4 locations — **disk wins**).

| Current | Target |
| --- | --- |
| `scene/bl3d_scene.h` / `.cpp` | `scene/scene.h` / `scene.cpp` |
| `scene/bl3d_object.h` | `scene/object.h` |
| `scene/bl3d_bas_struct.h` | `scene/vertex3d.h` |
| `scene/bl3d_sceneocttree.h` / `.cpp` | `index/octree.h` / `octree.cpp` |
| `scene/bl3d_sceneocttreenode.cpp` | `index/octree_node.cpp` (declaration stays in `octree.h` unless a colocated `octree_node.h` is clearly needed) |
| `scene/bl3d_vertexocttree.h` / `.cpp` | `index/vertex_octree.h` / `vertex_octree.cpp` |
| `scene/bl3d_vertextreenode.cpp` | `index/vertex_octree_node.cpp` |
| `dem/*`, `bridge/*`, `primitive/*`, `feature/*`, `surface/*`, `test/*` | **Keep** paths and semantic prefixes; only refresh includes that pointed at `bl3d_*` |

Include form after landing:

```cpp
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/render/scene3d/scene/object.h"
#include "legacy/render/scene3d/scene/vertex3d.h"
#include "legacy/render/scene3d/index/octree.h"
#include "legacy/render/scene3d/index/vertex_octree.h"
```

### 3.3 Approaches considered (locked A)

| Approach | Idea | Pros | Cons |
| --- | --- | --- | --- |
| **A. scene3d-only rename + vendor octree** (**locked**) | Prefix strip + `index/` + MIT header-only adapter | Small blast radius; ABI stable; matches dual-run layers | In-tree include churn |
| B. Move index into `gis::` / World | Spatial index becomes endgame API | Cleaner long-term | Reopens SP4; breaks leftover ABI timeline |
| C. Keep `bl3d_` names; only swap octree guts | Less rename noise | Leaves misleading / misspelled basenames | Fails approved naming goal |

**Locked: Approach A.**

---

## 4. Octree vendor + adapter boundary

### 4.1 Vendor choice

| Choice | Detail |
| --- | --- |
| **Library** | Prefer **[jbehley/octree](https://github.com/jbehley/octree)** (namespace `unibn`, MIT, **header-only** — commonly called unibn Octree / `Octree.hpp`). Pin a specific commit/tag under `third_party/` with LICENSE. |
| **Acceptable substitute** | Another **MIT (or Apache-2.0 / BSD-3) header-only** point/AABB octree of similar size if unibn cannot be vendored cleanly — still one header (or tiny set), no binary dep. Document the chosen repo + commit in `third_party/<name>/README.md` when landing. |
| **Forbidden** | PCL, OpenVDB, Embree, CGAL spatial packages, Assimp-as-scene-graph, dual-licensed / copyleft that contaminates `legacy_render`. |

GN: expose a thin `//third_party/<name>` (or existing third_party pattern) that only adds include dirs; `scene3d_sources` `deps` that target. No link of a second scene engine.

### 4.2 Adapter boundary

```
Callers (SmtScene, UI, tools, dem/bridge)
        │  still use SmtSceneOctTree / SmtVertexOctTree API
        ▼
index/octree.*  (+ vertex_octree.*)     ← LEGACY_RENDER_EXPORT types live here
        │  thin adapter: build/query/clear; map leftover AABB ↔ vendor points
        ▼
third_party/<octree>  (header-only)     ← no SmartGIS types, no device
```

**Rules:**

1. **Public leftover API** remains on `SmtSceneOctTree` / nodes (create from object AABB lists, frustum-related selection hooks that today sit on the node API, debug box flags as today). Internals may store vendor nodes or rebuild from leftover object pointers.
2. **`index/`** translates centers/widths/AABBs and object pointer lists; it does **not** take `LP3DRENDERDEVICE`. Device-bearing `Update*` / `Render*` / paint paths that currently live on octree nodes move to call sites in `scene/` (or stay as thin methods on the exported type that immediately delegate device work outside pure index state) — implementation plan must keep the **header dependency** graph clean: `index/*.h` must not include RHI device headers.
3. **`SmtVertexOctTree`** follows the same pattern under `index/vertex_octree.*` (vendor reuse or the same library’s point API); still exported as `SmtVertexOctTree`.
4. **Behavior:** rebuild / query / select semantics match current leftover behavior for existing tests and MFC 3D view paths (`CreateOctTreeSceneMgr`, scene manager recreate). No intentional culling policy change in this workstream.
5. **SP4 AABB mirror** (`seed_smt_scene_aabbs_into_world` / `CreateOctTreeSceneMgr` switch point) keeps calling the **same** `SmtScene` / octree entry points after rename — only include paths change.

---

## 5. Migration / dual-run notes

1. **One change set (or tightly sequenced commits on master):** `git mv` / rename → update `scene3d/BUILD.gn` sources → grep-fix every `#include` that names `bl3d_*` or old `scene3d/bl3d_*` (including stale `model3d/` / flat `scene3d/` leftovers if still present) → refresh `scene3d/README.md` layer list.
2. **Callers outside scene3d** (known today): `legacy/ui` (`view_3d`, `xcatalog/scenemgr`), `legacy/tool` (base3d / 3dviewctrl), `legacy/render` dem/bridge/primitive/feature/surface/rhi tests, and any remaining stale includes under absorbed tops. Update all in the same landing; **no** compatibility shims.
3. **Dual-run:** `legacy_render` remains the opt-in leftover DLL beside Views / GpuScene (dual-run design). This work does not change present ownership or force Views onto leftover GL.
4. **DEM unify:** do not move `dem_*` / `map_to_scene` / `scene_to_world` out of `dem/` / `bridge/`; only fix includes to `scene/object.h` etc.
5. **Verification (human):** `build.bat` (or targeted `legacy_render` + `dem_stereo_test` / existing scene tests). Agent does not run compilers.
6. **Docs on land:** touch `src/legacy/render/scene3d/README.md` and, if the as-built module table mentions `bl3d_`, `docs/build/src-layout.md` — same landing. Do **not** mark SP4 Success incomplete or rewrite its checkboxes.

---

## 6. Success criteria

1. [ ] Living design (this file) has Goal / Non-goals / Constraints / layout+rename / vendor+adapter / migration / Success / Out of scope / Related — no TBD placeholders.
2. [ ] On disk: no `bl3d_*` basenames under `scene3d/`; `index/` and `scene/` match §3; `octree` spelling used for scene index files.
3. [ ] `index/` headers do not include `LP3DRENDERDEVICE` / RHI device headers; device use stays in `scene/` or drawable modules.
4. [ ] Vendor pinned under `third_party/` (MIT-family header-only); adapter exposes unchanged `SmtSceneOctTree` (+ vertex peer) export surface.
5. [ ] `BUILD.gn` + all in-tree includes updated; **zero** old-path shim headers.
6. [ ] Behavioral parity: existing scene/DEM tests and leftover 3D view create-octree paths still pass when the human builds/tests.
7. [ ] SP4 Success / Done-when text left intact; umbrella Child table lists SP4b.

---

## 7. Out of scope / Follow-ons

| Item | Owner |
| --- | --- |
| Further `SmtScene` → `gis::World` / `GpuScene` thinning; octree query fully delegated to World | SP4 follow-on (parent spec Out of scope) |
| Present / Paint HWND / `bind_rhi_present` | SP2 |
| Host / Catalog / MFC chrome | SP3 |
| Shell compile gate / drop leftover from default product | SP5 |
| Renaming leftover `Smt*` types or PascalCase methods to snake_case en masse | Later cleanup — not SP4b |
| Point-cloud / terrain algorithm rewrites beyond include + index adapter | Out of scope |
| Implementation plan checkbox file | Separate `docs/superpowers/plans/` when user requests |

---

## 8. Related docs

- Umbrella: [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) (SP4b child row).
- Parent SP4: [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md).
- DEM unify: [`2026-09-19-leftover-scene3d-dem-unify-design.md`](2026-09-19-leftover-scene3d-dem-unify-design.md).
- Dual-run / P2.4 scene3d layers: [`2026-09-27-legacy-render-subdirectory-dual-run-design.md`](2026-09-27-legacy-render-subdirectory-dual-run-design.md).
- As-built: [`../../build/src-layout.md`](../../build/src-layout.md), [`../../../src/legacy/render/scene3d/README.md`](../../../src/legacy/render/scene3d/README.md).

---

## Spec self-review (2026-09-28)

| Check | Result |
| --- | --- |
| Placeholders | No TBD/TODO/“later decide” gaps in locked choices; vendor allows one documented MIT substitute if unibn pin fails at land time. |
| vs SP4 | Explicitly does not rewrite SP4 Success; forbids wholesale World replacement; keeps AABB mirror call sites. |
| Scope | Scheme A boundary restated in Goal, Non-goals, Constraints, and Out of scope. |
| Ambiguity | Device vs index: headers in `index/` must not include device; exported methods that historically took a device may remain on the type but must not pull device headers into `index/*.h` — clarified in §4.2. |
| README Active | `docs/superpowers/README.md` does not exist; umbrella Child table is the living index row for SP4b. |
| Fixes applied in-file | Corrected vendor repo to `jbehley/octree` (`unibn` namespace); confirmed no conflict wording against landed SP4 Success. |
