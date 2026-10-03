<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP4b 鈥?scene3d `index/` + unibn octree Implementation Plan


> **Design living:** SP decisions live in [../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md). This file is the checklist only.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Under `src/legacy/render/scene3d` only, drop `bl3d_` basenames, add `index/` (no RHI device includes in headers), vendor MIT header-only [jbehley/octree](https://github.com/jbehley/octree) (`unibn`), and keep exported `SmtSceneOctTree` / `SmtVertexOctTree` behavior via a thin adapter.

**Architecture:** Scheme A. Spatial index TUs live in `scene3d/index/`; `scene/` keeps `SmtScene` / `Smt3DObject` / `vertex3d`. Adapter `.cpp` files include `Octree.hpp` and own a heap `unibn::Octree` of point centers (scene object AABB centers / vertex positions) while the exported node walk (frustum render/select) stays behavior-compatible. GN: `//third_party/octree` 鈫?`.src/octree` include; `scene3d_sources` deps that label.

**Tech Stack:** C++23 MSVC, GN/Ninja (`out/`), leftover `legacy_render` DLL, unibn `Octree.hpp` (MIT, header-only).

## Global Constraints

- Work on **master** only; **no** feature branch; **do not commit** unless the user asks.
- Agent **must not** run `build.bat` / `gn` / `ninja` / any compile or test exe (human owns verify).
- Colocated `.h` next to `.cpp`; nesting cap `legacy/render/scene3d/<module>/`; **no** old-path forwarding shims.
- Keep `Smt*`, `LEGACY_RENDER_EXPORT`, `dll_stem = legacy_render`. New helpers: `snake_case`. Comments in English; copyright year **2026** on touched Mogu headers.
- `index/*.h` must **not** `#include` `render_device.h` (or other full device API headers). Forward-declare `Smt3DRenderDevice` / `LP3DRENDERDEVICE` and `SmtFrustum` as needed; device use stays in `.cpp` / `scene/`.
- No Qt; no PCL/OpenVDB/Embree/Assimp-as-octree.

## File structure (target)

```
third_party/octree/BUILD.gn
third_party/octree/README.md
third_party/manifest.json          # +octree package pin
third_party/BUILD.gn               # group("octree")
third_party/gn/BUILD.gn            # facade group
third_party/.src/octree/           # fetch target (gitignored); Octree.hpp + LICENSE

src/legacy/render/scene3d/
  index/octree.h|.cpp|octree_node.cpp
  index/vertex_octree.h|.cpp|vertex_octree_node.cpp
  scene/scene.h|.cpp|object.h|vertex3d.h
  BUILD.gn, README.md
  (dem|bridge|primitive|feature|surface|test unchanged paths)
```

### Rename map

| Current | Target |
| --- | --- |
| `scene/bl3d_scene.h` / `.cpp` | `scene/scene.h` / `scene.cpp` |
| `scene/bl3d_object.h` | `scene/object.h` |
| `scene/bl3d_bas_struct.h` | `scene/vertex3d.h` |
| `scene/bl3d_sceneocttree.h` / `.cpp` | `index/octree.h` / `octree.cpp` |
| `scene/bl3d_sceneocttreenode.cpp` | `index/octree_node.cpp` |
| `scene/bl3d_vertexocttree.h` / `.cpp` | `index/vertex_octree.h` / `vertex_octree.cpp` |
| `scene/bl3d_vertextreenode.cpp` | `index/vertex_octree_node.cpp` |

Include form:

```cpp
#include "legacy/render/scene3d/scene/scene.h"
#include "legacy/render/scene3d/scene/object.h"
#include "legacy/render/scene3d/scene/vertex3d.h"
#include "legacy/render/scene3d/index/octree.h"
#include "legacy/render/scene3d/index/vertex_octree.h"
```

---

### Task 1: Vendor unibn octree (Workstream V)

**Files:**
- Create: `third_party/octree/BUILD.gn`
- Create: `third_party/octree/README.md`
- Modify: `third_party/manifest.json` (add `octree` package)
- Modify: `third_party/BUILD.gn`, `third_party/gn/BUILD.gn`
- Populate: `third_party/.src/octree/` via `git clone` / `tools/fetch.py` (gitignored source tree)

**Interfaces:**
- Produces: GN label `//third_party:octree` 鈫?`//third_party/gn:octree` 鈫?`//third_party/octree:octree` with `include_dirs += [ "//third_party/.src/octree" ]` so TUs can `#include "Octree.hpp"`.

- [x] **Step 1: Fetch upstream into `.src/octree`**

```bat
git clone https://github.com/jbehley/octree.git third_party/.src/octree
git -C third_party/.src/octree rev-parse HEAD
```

Expected: tree contains `Octree.hpp` + `LICENSE`. Record commit SHA in README + manifest `git_ref`.

- [x] **Step 2: Thin GN wrapper**

`third_party/octree/BUILD.gn`:

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

config("octree_headers") {
  include_dirs += [ "//third_party/.src/octree" ]
}

group("octree") {
  public_configs = [ ":octree_headers" ]
}
```

Wire `group("octree")` in `third_party/BUILD.gn` and `third_party/gn/BUILD.gn` (same pattern as `eigen` / `tinygltf`).

- [x] **Step 3: Manifest + README**

Add package (install_skip header-only):

```json
{
  "name": "octree",
  "git_url": "http://localhost:3000/ccl/octree.git",
  "git_url_fallbacks": [
    "https://github.com/jbehley/octree.git"
  ],
  "git_ref": "<pinned-sha-or-master-tip>",
  "install_skip": true,
  "note": "jbehley/octree unibn Octree.hpp MIT header-only. Sources in .src/octree; thin GN at third_party/octree. Used by legacy/render/scene3d/index adapter only."
}
```

`third_party/octree/README.md`: upstream URL, MIT, pin SHA, include `#include "Octree.hpp"`, GN label.

- [x] **Step 4: Verify on disk (no compile)**

Confirm `third_party/.src/octree/Octree.hpp` and `LICENSE` exist. If network fetch failed: document blocker in README; leave GN hooks; Task 3 may temporarily keep hand-rolled body with `// TODO(sp4b): wire unibn` until fetch succeeds.

---

### Task 2: Rename + `index/` / `scene/` split (Workstream R)

**Files:**
- Create dir: `src/legacy/render/scene3d/index/`
- `git mv` per rename map above
- Modify moved headers/sources for new include paths + include guards
- Modify: `src/legacy/render/scene3d/BUILD.gn`

**Interfaces:**
- Consumes: Task 1 `//third_party:octree`
- Produces: on-disk layout matching spec 搂3; public types unchanged

- [x] **Step 1: `git mv` rename map**

```bat
mkdir src\legacy\render\scene3d\index
git mv src/legacy/render/scene3d/scene/bl3d_scene.h src/legacy/render/scene3d/scene/scene.h
git mv src/legacy/render/scene3d/scene/bl3d_scene.cpp src/legacy/render/scene3d/scene/scene.cpp
git mv src/legacy/render/scene3d/scene/bl3d_object.h src/legacy/render/scene3d/scene/object.h
git mv src/legacy/render/scene3d/scene/bl3d_bas_struct.h src/legacy/render/scene3d/scene/vertex3d.h
git mv src/legacy/render/scene3d/scene/bl3d_sceneocttree.h src/legacy/render/scene3d/index/octree.h
git mv src/legacy/render/scene3d/scene/bl3d_sceneocttree.cpp src/legacy/render/scene3d/index/octree.cpp
git mv src/legacy/render/scene3d/scene/bl3d_sceneocttreenode.cpp src/legacy/render/scene3d/index/octree_node.cpp
git mv src/legacy/render/scene3d/scene/bl3d_vertexocttree.h src/legacy/render/scene3d/index/vertex_octree.h
git mv src/legacy/render/scene3d/scene/bl3d_vertexocttree.cpp src/legacy/render/scene3d/index/vertex_octree.cpp
git mv src/legacy/render/scene3d/scene/bl3d_vertextreenode.cpp src/legacy/render/scene3d/index/vertex_octree_node.cpp
```

- [x] **Step 2: Fix include guards + self-includes in moved files**

Replace `_BL3D_*` guards with `LEGACY_RENDER_SCENE3D_INDEX_OCTREE_H` style (or `SCENE3D_SCENE_H`). Update every `#include "legacy/render/scene3d/scene/bl3d_..."` inside scene3d to the new paths.

- [x] **Step 3: Device-free `index/*.h`**

In `index/octree.h` and `index/vertex_octree.h`:

- Remove `#include "legacy/render/rhi3d/public/device/render_device.h"` (and videobuffer from vertex header if only used in `.cpp`).
- Forward-declare:

```cpp
namespace render {
class Smt3DRenderDevice;
typedef Smt3DRenderDevice* LP3DRENDERDEVICE;
class SmtFrustum;
class SmtVertexBuffer;  // vertex_octree.h only
}
```

- Keep `#include "legacy/render/scene3d/scene/object.h"` / `vertex3d.h` / math as needed for `vSmt3DObjectPtrs` and inheritance.
- Thin `scene/object.h`: replace `#include "鈥?render_device.h"` with the same forward decl so index 鈫?object does not pull the full device API header.

- [x] **Step 4: Update `scene3d/BUILD.gn` sources + deps**

```gn
sources = [
  "scene/scene.cpp",
  "index/octree.cpp",
  "index/octree_node.cpp",
  "index/vertex_octree.cpp",
  "index/vertex_octree_node.cpp",
  # 鈥?dem/bridge/primitive/feature/surface unchanged 鈥?
]
deps += [ "//third_party:octree" ]
```

---

### Task 3: Thin unibn adapter (Workstream R, continues)

**Files:**
- Modify: `index/octree.h`, `index/octree.cpp`, `index/octree_node.cpp`
- Modify: `index/vertex_octree.h`, `index/vertex_octree.cpp`, `index/vertex_octree_node.cpp`

**Interfaces:**
- Consumes: `unibn::Octree` from `Octree.hpp`
- Produces: unchanged public methods on `SmtSceneOctTree` / `SmtVertexOctTree` / nodes

- [x] **Step 1: Point traits + opaque index holder**

In `octree.cpp` (anonymous namespace or `render::detail`):

```cpp
#include "Octree.hpp"

struct UnibnVec3 {
  float x, y, z;
};

struct SceneOctreePointIndex {
  std::vector<UnibnVec3> points;
  unibn::Octree<UnibnVec3> tree;
  void clear() {
    points.clear();
    tree = unibn::Octree<UnibnVec3>();
  }
  void rebuild_from_objects(const vSmt3DObjectPtrs& objects) {
    points.clear();
    points.reserve(objects.size());
    for (Smt3DObject* obj : objects) {
      if (!obj) continue;
      const Vector3& c = obj->GetAabb().vcCenter;
      points.push_back(UnibnVec3{static_cast<float>(c.x),
                                 static_cast<float>(c.y),
                                 static_cast<float>(c.z)});
    }
    if (!points.empty()) tree.initialize(points);
  }
};
```

Add protected opaque member on `SmtSceneOctTree`: `SceneOctreePointIndex* m_point_index;` (allocated in ctor, deleted in dtor). Same pattern for `SmtVertexOctTree` with vertex positions.

- [x] **Step 2: Wire build/destroy**

In `CreateOctTree`: after existing root `CreateNode(鈥?` (keep node tree for frustum render/select parity), call `m_point_index->rebuild_from_objects(v3DObjectPtrs)`.

In `DestroyTree`: `m_point_index->clear()` then delete root node as today.

- [x] **Step 3: Use unibn on point queries where mapped**

`SmtVertexOctTree::HitTestOctNode`: if `m_point_index` non-empty, `findNeighbor` / `radiusNeighbors` with epsilon radius; fall back to prior node walk if index empty.

Keep `Render*` / `Update*` / `Select*` node walks unchanged (they need device + frustum); implementations stay in `.cpp` and may `#include` device headers there.

- [x] **Step 4: Header comment**

Short English class comment on `SmtSceneOctTree` / `SmtVertexOctTree`: leftover export surface; spatial point index via unibn behind adapter.

---

### Task 4: In-tree include / caller migration (Workstream C 鈥?after R)

**Files (known callers; re-scan with path-scoped search):**
- `scene3d/{dem,bridge,primitive,feature,surface,test}/**`
- `legacy/ui/viewport/view_3d.h`, `legacy/ui/catalog/scene/scenemgr.h`
- `legacy/tool/base/base3dtool.h`, `legacy/tool/nav/3dviewctrltool.{h,cpp}`
- `legacy/render/rhi3d/impl/gl/test/map_paint_test.cc`
- Any remaining `bl3d_` or flat `scene3d/bl3d_*` includes

- [x] **Step 1: Replace includes**

Map:

| Old | New |
| --- | --- |
| `鈥?scene/bl3d_scene.h` | `鈥?scene/scene.h` |
| `鈥?scene/bl3d_object.h` | `鈥?scene/object.h` |
| `鈥?scene/bl3d_bas_struct.h` | `鈥?scene/vertex3d.h` |
| `鈥?scene/bl3d_sceneocttree.h` | `鈥?index/octree.h` |
| `鈥?scene/bl3d_vertexocttree.h` | `鈥?index/vertex_octree.h` |
| `鈥?scene3d/bl3d_*.h` (stale flat) | corresponding `scene/` or `index/` path |

- [x] **Step 2: Confirm zero `bl3d_` under `src/` and no shim headers**

Path-scoped search for `bl3d_` must be empty (except archive docs if any 鈥?do not rewrite archived bodies).

---

### Task 5: Docs + Success checklist

**Files:**
- Modify: `src/legacy/render/scene3d/README.md` (add `index/` layer; note unibn; drop `bl3d_` basenames)
- Modify only if needed: `docs/build/src-layout.md` (no `bl3d_` mention today 鈥?skip unless inaccurate)
- Modify: `docs/superpowers/specs/2026-09-28-scene3d-index-octree-design.md` Plan line 鈫?this plan path

- [x] **Step 1: README layer list**

State layers: `scene/` `index/` `primitive/` `feature/` `surface/` `dem/` `bridge/` `test/`. Note vendor `//third_party:octree` (unibn).

- [x] **Step 2: Spec Plan pointer**

Set spec metadata `Plan:` to `docs/superpowers/plans/2026-09-28-scene3d-index-octree.md`.

- [x] **Step 3: Success criteria (agent self-check; human builds)**

| # | Criterion | Agent check |
| --- | --- | --- |
| 1 | Spec complete | already accepted |
| 2 | No `bl3d_*` under scene3d; `index/`+`scene/`; `octree` spelling | disk + search |
| 3 | `index/*.h` no `render_device.h` | header scan |
| 4 | Vendor under `third_party/` + adapter exports `SmtSceneOctTree` | `.src/octree` + GN |
| 5 | BUILD.gn + all includes; no shims | search |
| 6 | Behavioral parity | **human:** `build.bat` (+ `dem_stereo_test` / leftover 3D view) |
| 7 | SP4 Success text intact | do not edit SP4 Success checkboxes |

---

### Task 6: Replace hand-rolled node tree with flat + unibn (approach A)

**Files:**
- Modify: `index/octree.h|.cpp`, `index/vertex_octree.h|.cpp`, `scene3d/BUILD.gn`, README, umbrella §13
- Delete: `index/octree_node.cpp`, `index/vertex_octree_node.cpp`

**Interfaces:**
- Keep exported `SmtSceneOctTree` / `SmtVertexOctTree` methods
- Remove exported `SmtSceneOctTreeNode` / `SmtVertexOctTreeNode`

- [x] **Step 1: Flat scene octree** — object list + unibn; Render/Select/Update scan; scene AABB debug cube
- [x] **Step 2: Flat vertex octree** — single VB + unibn; HitTest unibn-only; RenderTree draws VB
- [x] **Step 3: Drop node TUs from BUILD.gn; delete node sources**
- [x] **Step 4: Docs** — umbrella §13 Locked note; scene3d README as-built line

---

### Task 7: Point-cloud upgrade (P0 query / P1 Cloud owns VB / P2 chunks)

**Files:**
- Modify: `index/vertex_octree.h|.cpp`, `surface/pointcloud.h|.cpp`, umbrella §13, README

- [x] **P0** — `build` / `hit_test` / `find_nearest` / `radius_neighbors`; index has no VB
- [x] **P1** — `Smt3DPointCloud` owns VB + draw; index query-only via `m_point_index`
- [x] **P2** — N≥200k reorder into spatial chunks; frustum cull per chunk draw range

---

## Parallel execution notes

| Stream | Tasks | Paths |
| --- | --- | --- |
| **V** | Task 1 | `third_party/**` only |
| **R** | Tasks 2鈥? | `src/legacy/render/scene3d/**` |
| **C** | Task 4 | callers outside + remaining scene3d includes after R moves |
| **D** | Task 5 | docs + README |

Serialize: **R before C** for include path correctness. V 鈭?R OK.

## Human verification (agent does not run)

```bat
build.bat
build.bat dem_stereo_test
```

Optional: exercise leftover 3D view create-octree path (`CreateOctTreeSceneMgr`) manually.

## Spec coverage self-review

| Spec 搂 | Task |
| --- | --- |
| Goal scheme A / drop bl3d_ / octree spelling | 2 |
| index/ vs scene/ device boundary | 2鈥? |
| Vendor jbehley/octree + adapter | 1, 3 |
| Same-change includes + BUILD.gn | 2, 4 |
| README / as-built | 5 |
| No shims / ABI stable | 2鈥? |
| Success checklist | 5 Step 3 |
