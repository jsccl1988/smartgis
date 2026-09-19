<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP3 Host behavior extract — Implementation Plan

> **For agentic workers:** follow checkboxes; stay on `master`; no commit unless asked.

**Goal:** Extract HWND-free host session helpers into `content` (attribute writeback + Catalog LayerDesc JSON); wire `MapScene` / CEF / Views; keep MFC dlg thin.  
**Spec:** [`../specs/2026-09-19-legacy-host-behavior-extract-design.md`](../specs/2026-09-19-legacy-host-behavior-extract-design.md)

## Constraints

- Path partition: `src/content/**`, `src/app/views/**`, `src/app/cef/**`（Catalog JSON 接线）, `src/legacy/ui/**`（薄注释）, docs; thin legacy comments only.
- No Qt; English source comments; copyright 2026; `snake_case` new APIs.
- Do not touch `src/legacy/tool/**` or `src/legacy/render/**` (bind_workspace 薄接线已在 `legacy/ui/xview`).

---

### Task 1: `content::feature_attrs` + test

**Files:**
- Create: `src/content/public/feature_attrs.h`
- Create: `src/content/feature_attrs.cc`
- Create: `src/content/feature_attrs_test.cc`
- Modify: `src/content/BUILD.gn`

**Steps:**
1. [x] Add `NamedField { name, value }`, `encode_feature_token`, `decode_feature_token`, `apply_named_field`.
2. [x] Token format: `"fid:"` + lowercase hex of `FeatureId` bytes (match existing MapScene).
3. [x] `apply_named_field`: update existing name or append; reject empty field name.
4. [x] GN `test("content_feature_attrs_test")` deps `:content`.
5. [x] Run `ninja -C out content_feature_attrs_test` (or `build.bat` equivalent) when building.

---

### Task 2: Wire `MapScene` + clean `browser_view`

**Files:**
- Modify: `src/app/views/map_scene.cc` (delegate token / update)
- Modify: `src/app/views/browser_view.cc` (remove unused `detail::feature_id_from_opaque_token`)
- Optional: `src/legacy/ui/gui/dlg_2d_feature_info.h` English comment pointing at content API

**Steps:**
1. [x] Include `content/public/feature_attrs.h`.
2. [x] `feature_token` / `feature_id_from_token` / `update_feature_field` call content helpers.
3. [x] Delete dead opaque-token helpers in `browser_view.cc` detail namespace.
4. [x] Leftover dlg comment: product writeback → `content::feature_attrs`.

---

### Task 3: Catalog `LayerDesc` JSON

**Files:**
- Create: `src/content/public/catalog_layers.h`
- Create: `src/content/catalog_layers.cc`
- Create: `src/content/catalog_layers_test.cc`
- Modify: `src/content/BUILD.gn`
- Modify: `src/app/views/map_scene.h` (`using LayerDesc = content::LayerDesc`)
- Modify: `src/app/views/browser_view.cc` (sync via `content::LayerDesc`)
- Modify: `src/app/cef/chrome_bridge.cc` (delegate `layers_to_catalog_json`)

**Steps:**
1. [x] Add `content::LayerDesc` + `json_escape_string` + `layers_to_catalog_json` (CEF wire: id/name/visible; no `active`).
2. [x] GN `test("content_catalog_layers_test")`.
3. [x] `MapScene::LayerDesc` aliases `content::LayerDesc`; CEF / Views sync use content helpers.
4. [x] `bind_workspace`：已在 `legacy/ui/xview` 薄接线，本轮不改 `legacy/tool`。

**Deferred (still out of this task):**
- Edit draft commit (`append_from_draft` viewport seam) — SP1 tool / later.
- `CatalogCall` / `CatalogDelta` pipe payload beyond snapshot JSON — later incremental.
- SP5 默认停编 leftover 闸门。
