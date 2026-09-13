# Final review — OGR database datasource replacement

**Range:** `c224770` … `c1d4774` on `master` (HEAD `c1d4774092bc0e0f368e1c74fd6a26a681565774`), plus adjacent C++ standard commits `daf75ca` (C++20 rules) and `16f3cb9` (C++23 bump).  
**Spec:** `docs/superpowers/specs/2026-09-13-ogr-db-datasource-design.md`  
**Plan:** `docs/superpowers/plans/2026-09-13-ogr-db-datasource.md`  
**Review mode:** read-only. Did **not** re-run `build.bat`. Confirmed `//src:src_all` deps via `gn desc out //src:src_all deps --all` on the existing `out/` graph.

This is a whole-branch review, not a merge-PR gate.

---

### Strengths

- **One vector class + traits, not seven ADO `*fcls` copies.** `feature_kind_traits<Ft>` plus `visit_feature_kind` / `for_each_extra_field` drive `OgrVectorLayer::Create` WKB and extra fields (`ogr_feature_kind.h:136-167`, `ogr_vec_layer.cc:34-48`). `db_provider_traits<Provider>` holds GPKG / PostgreSQL / SpatiaLite open targets (`ogr_connect.h:14-40`).
- **Manager really instantiates OGR.** Both `CreateTmpDataSource` and `CreateDataSource` for `DS_DB_ADO` do `new sdb::datasource::OgrDataSource()` (`datasourcemgr.cpp:174-177`, `221-224`). No `SmtAdoDataSource`, no `comsuppw.lib`.
- **ACCESS / SQL Server Open is hard-false.** Unsupported providers never call GDAL (`ogr_dataset.cc:153-157`). Tests cover it (`sde_gdal_test.cc:418-422`).
- **Shared codec is real and SMF calls it.** `CopyOGRFeaToSmtFea` → `copy_ogr_feature_to_smt`, then SMF-only default style + random pen/brush (`smf_ogrsupport.cpp:60-97`). Random colors stay out of the codec, as specified.
- **ADO is actually off `src_all`.** `src/BUILD.gn` `src_all` has no `//src/ado:ado`. `src/sdb/BUILD.gn` and `src/sdb/datasource/BUILD.gn` list `sde_gdal`, not `sde_ado`. `gn desc out //src:src_all deps --all` matches **zero** `ado` / `sde_ado` targets. Leftover `src/ado` and `sdb/datasource/ado` BUILD files remain on disk only (their sole GN edge is `sde_ado` → `//src/ado:ado`, unreferenced).
- **Docs and app default were updated in the same change set.** Root README, `src/README.md`, `docs/build/src-layout.md`, and `docs/README.md` state the OGR DB path. `app_smtapp.cpp` looks for `sample1.gpkg` and sets `PROVIDER_GPKG`.
- **Namespaces and new-file headers match the new-tree rules** (`sdb::datasource`, snake_case, 2026 Mogu copyright on `src/sdb/datasource/gdal/*`).
- **No SOCI / nanodbc / second GDAL / Qt.** Raster create is `SMT_ERR_UNSUPPORTED` rather than a private `geom_points` blob table (`ogr_raster_layer.cc:45-49`).

---

### Issues

#### Critical (Must Fix)

_(none that crash or silently destroy existing user data on the default path.)_  
Empty catalog when `sample1.gpkg` is missing is specified. Existing Access `.mds` entries fail Open and are dropped — that is the intended ACCESS reject, not a regression in the new code.

#### Important (Should Fix)

1. **GPKG / SpatiaLite Open is not GPKG — Shapefile directory fallback (spec gap)**  
   - File: `src/sdb/datasource/gdal/ogr_dataset.cc:72-104`, `169-188`  
   - Issue: This `gdal_sdk` has no GPKG / SQLite / PostgreSQL drivers (implementer claim; consistent with the fallback and with `src/README.md:49`). `file_create_driver` rewrites the open target to a path-without-extension and `Create`s an **ESRI Shapefile directory**. Tests named “GPKG Create/Open” never assert a `.gpkg` file, the `GPKG` driver, or `GDALGetDriverShortName`.  
   - Why it matters: Spec success is “GPKG still works if PostgreSQL is missing” and “create a temp `.gpkg`”. Product `PROVIDER_GPKG` writes a folder of `.shp`/`.dbf` instead of a GeoPackage. No transactions, no same-file raster, Shapefile field/geometry limits. `sde_gdal_test` can stay green forever without a GPKG driver.  
   - Fix: Treat missing GPKG as **Open/Create failure** with the already-logged driver list (matches the PostgreSQL path). Keep Shapefile only behind an explicit test-only env (e.g. `SMT_ALLOW_SHAPEFILE_FALLBACK`) **or** rebuild `gdal_sdk` with GPKG/SQLite. Add `expect(GetDriverByName("GPKG") || getenv("SMT_ALLOW_SHAPEFILE_FALLBACK"), …)` and assert the created artifact is a `.gpkg` when the driver exists.

2. **App sample path strips `.gpkg`, so a real sample file is never the open target**  
   - File: `src/app/app_core/app_smtapp.cpp:352-363`  
   - Issue: `path_is_file(...\sample1.gpkg)` then `SplitFileName` + `strcpy(info.db.szDBName, szTitle)`. `SplitFileName` puts the stem in `title` and `title+ext` in `fileName` (`src/base/api.cpp:412-426`). Spec table: `szDBName` = `name.gpkg`. Resulting target is `dir\sample1`, not `dir\sample1.gpkg`.  
   - Why it matters: Even after a GPKG-capable SDK ships, `InitSmtDataSource` will not open the file it just found. Combined with (1), it creates/opens a Shapefile directory named `sample1` beside the `.gpkg`.  
   - Fix: `strcpy(info.db.szDBName, szFileName)` (or `sample1.gpkg`).

3. **`sde_gdal` DLL is a stub; `ogr_codec` swallowed the device**  
   - File: `src/sdb/datasource/gdal/BUILD.gn:7-47`  
   - Issue: Spec put dataset / layers / codec in `smt_shared_library("sde_gdal")` and a **thin** `ogr_codec` source_set. Implementation compiles `ogr_dataset.cc`, `ogr_vec_layer.cc`, `ogr_raster_layer.cc` into `ogr_codec`. `sde_gdal` only has `gdal_driver.cc`. `register_gdal_driver()` is never called (`Open` uses `GDALAllRegister` directly). SMF and mgr **statically** link the whole DB device.  
   - Why it matters: `SmtSDEGdalDevice` is not the runtime provider. SMF pays for dataset/layer TUs it should not own. Layering will get worse when someone LoadLibrary-s the DLL and finds no `OgrDataSource`.  
   - Fix: Move dataset/layer sources to `sde_gdal` (export or keep mgr linking that target only). Leave `ogr_codec` as connect + feature_kind + codec. Call `register_gdal_driver()` from `Open`.

4. **Style field (`style` / `OFTBinary`) is not created or copied**  
   - File: `ogr_vec_layer.cc:43-46`, `ogr_feature_codec.cc:200-277`  
   - Issue: Spec: style remains a product binary field named `style`. Create only adds traits extra fields. Attribute copy skips `OFTBinary` / `SmtBinary`.  
   - Why it matters: DB round-trip drops cartography that ADO stored. SMF papers over this with default + random colors after decode — DB path does not.  
   - Fix: Create `style` as `OFTBinary` on vector layers; copy blob ↔ `SmtFeature` style in the codec.

5. **Codec cannot decode MultiLineString / MultiPolygon as curve/surface**  
   - File: `ogr_feature_codec.cc:92-151`, `37-54`  
   - Issue: `infer_feature_type` maps `wkbMultiLineString` → Curve and `wkbMultiPolygon` → Surface, but `decode_linestring` / `decode_polygon` require single-part WKB. TIN decode handles MultiPolygon; Surface does not.  
   - Why it matters: PostGIS and many Shapefile polygons arrive as Multi*. Fetch will drop those features (`ogr_vec_layer.cc:82-86`).  
   - Fix: Take first part or flatten; mirror TIN’s MultiPolygon walk for Surface.

6. **Query never uses OGR filters; attribute query ignored**  
   - File: `ogr_vec_layer.cc:134-155`  
   - Issue: Spec: `SetSpatialFilter` / `SetAttributeFilter`, then mem fallback. Implementation only scans the in-memory `features_` cache and ignores `SmtPQueryDesc`.  
   - Why it matters: Unfetched layers miss hits; large layers never use the provider spatial index.  
   - Fix: Apply envelope to `SetSpatialFilter`, pass simple attribute SQL when possible, then codec into `pQueryResult`.

7. **`DeleteFeature` ignores the OGR error; `CreateSpatialIndex` lies**  
   - File: `ogr_vec_layer.cc:130-132`, `204-216`  
   - Issue: `layer_->DeleteFeature(...)` result is discarded; cache erase can still return `SMT_ERR_NONE`. `CreateSpatialIndex` returns success and does nothing.  
   - Why it matters: UI/edit thinks the row is gone or indexed when the file is unchanged.  
   - Fix: Propagate `OGRERR_*`. Return `SMT_ERR_UNSUPPORTED` (or call `CreateIndex` when the driver can) for spatial index.

8. **Raster/child-image is a stub even when bands exist**  
   - File: `ogr_raster_layer.cc:23-65`; `ogr_dataset.cc:217-234`  
   - Issue: Spec: wrap `GDALDataset` raster / list raster bands in `GetLayerTableInfos`. `Create()` is true if `GetRasterCount()>0`, but `GetRaster` / `CreaterRaster` stay `SMT_ERR_UNSUPPORTED`. `fill_layer_infos` walks vector layers only.  
   - Why it matters: Child-image and rasters are first-class in the spec; they are name-only objects here. UNSUPPORTED on **create** is allowed; claiming Open/Create success then failing every I/O is not.  
   - Fix: Either read/write bands or keep Create/Open false whenever I/O is UNSUPPORTED. List rasters in `fill_layer_infos`.

#### Minor (Nice to Have)

1. **`gdal_driver.h` comments are stale** (`gdal_driver.h:7-18`) — still describes a future seam “beside ado”.  
2. **`src-layout.md` DLL table still lists `SmtSDEAdoDevice` and omits `SmtSDEGdalDevice`** (`docs/build/src-layout.md:103-106`). Prose is correct; the table is not.  
3. **`build/BUILD.gn` `legacy` include_dirs still add `sdb/datasource/ado` and `src/ado`** (`build/BUILD.gn:49`, `75`) — leftover `-I` only, not a `src_all` link.  
4. **SMF still contains unused `CopyOGRGeomToSmtGeom` / `CopyOGRAttToSmtAtt`** (`smf_ogrsupport.cpp:100-208`) after the codec switch.  
5. **Open failure does not log a password-redacted target** (`ogr_dataset.cc:190-193`); spec asked for driver + redacted target + CPL message.  
6. **PostgreSQL `host:port` uses `rfind(':')`** (`ogr_connect.cc:29-32`) — breaks IPv6. Password is unescaped in `PG:`.  
7. **`strncpy` of layer names without forcing a trailing NUL** (`ogr_dataset.cc:229-230`).  
8. **Polygon holes and Z are dropped** (exterior ring, X/Y only).  
9. **`feature_type_from_layer` treats polygon without an `area` field as TIN** (`ogr_dataset.cc:123-129`) — hostile to foreign polygon layers.  
10. **TIN decode does not accept `wkbTIN`** (`ogr_feature_codec.cc:375-388`) even though infer maps it to `SmtFtTin`.  
11. **Tests do not check persisted FID** (spec: IDs match). Anno Memory-driver test does not check `color` / `angle` values.  
12. **`CPLSetErrorHandler` is process-global** (`ogr_dataset.cc:46-51`) — can steal SMF/GDAL logs.  
13. **Task 8 checked in the entire SMF tree** as new files (2010 headers). Codec include is the real change; the rest is promotion noise.

---

### ADO off `src_all`?

**Yes.** Link graph for `//src:src_all` / `//src/sdb:datasource` is `sde_gdal` + mem/smf/ws/mgr. `//src/ado:ado` and `//src/sdb/datasource/ado:sde_ado` are not dependencies. `gn desc out //src:src_all deps --all` has no ado hits.

Caveats (not src_all, but leftover):

- Sources and `BUILD.gn` for ADO remain (specified).  
- `legacy` still `-I`s those dirs.  
- `16f3cb9` re-added `src/ado/ado_adorecordset.cpp` as part of the C++23 sweep — compile-only, not wired into `all`.

---

### Traits / C++20 / C++23?

| Question | Answer |
| --- | --- |
| Provider + feature-kind **traits** as specified? | **Yes.** Specializations + `visit_feature_kind` + tuple/`index_sequence` field walk. No seven layer classes. |
| C++20 **concepts / requires** in the OGR TUs? | **No.** `if constexpr` on `OGRFieldType` only (`ogr_feature_codec.cc:166-172`). Fold in `for_each_extra_field` is C++17. |
| Did this change bump `cc_std`? | **Not inside the OGR commits.** Adjacent `daf75ca` / `16f3cb9` raised the **tree** to C++20 then C++23. The OGR spec/plan said stay on C++17 and not bump `cc_std`. Current workspace rule is C++23; the OGR code is traits-style C++17 that still compiles. |
| C++20 used to collapse 2D/3D geometry? | **No** — out of scope; codec is still `dynamic_cast` + WKB switches. |

Deviation vs original plan (“no concepts, no cc_std bump”) is a **tree-wide** standard bump, not an OGR TMP rewrite. Acceptable if the C++23 rule is now normative; do not treat missing concepts as a defect.

---

### GPKG vs Shapefile spec gap

| Spec / plan | Implementation on this SDK |
| --- | --- |
| New files are GeoPackage; `GDALOpenEx` on `dir\name.gpkg`; Create with GPKG driver if missing | GPKG driver absent → ESRI Shapefile **directory** beside the `.gpkg` path |
| “If SDK lacks PostgreSQL, **GPKG still works**” | PostgreSQL **and** GPKG/SQLite missing. File create does **not** fail; it disguises Shapefile as GPKG |
| Always-on test: create a temp `.gpkg`, round-trip | Tests pass on a Shapefile folder; no assertion that a `.gpkg` exists |
| App default `PROVIDER_GPKG` + `sample1.gpkg` | Provider flag is GPKG; `szDBName` is `sample1` (no extension); Open will not hit `sample1.gpkg` |

This is the main product gap. Documenting it in `src/README.md` and progress notes is honest; it does **not** satisfy the spec’s GPKG success bar. Shapefile fallback is a reasonable **lab** escape hatch, not a shippable GPKG provider.

---

### Recommendations

- Rebuild or enable GPKG (+ SQLite / PostgreSQL) in `gdal_sdk`, **or** fail Open when the requested driver is absent. Do not ship “GPKG” as Shapefile.  
- Split `ogr_codec` vs `sde_gdal` as the plan’s file map.  
- Add one test that fails if `GetDriverByName("GPKG")` is null **unless** an explicit fallback env is set; when GPKG exists, assert the file header / driver short name.  
- Persist `style`; decode Multi* geometries; wire `SetSpatialFilter`.  
- Fix `szDBName` to `sample1.gpkg` before anyone drops a sample file in `data/data source/db/`.  
- Update the `src-layout.md` DLL table row for `SmtSDEGdalDevice`.

---

### Task quality

| Task | Quality |
| --- | --- |
| 1 Providers / open targets | High. Traits + tests match the brief. |
| 2 Codec (point/line/polygon/anno) | High. Memory-driver anno test is real. |
| 3–6 Dataset / layers / TIN / grid / raster | One squashed commit (`0ee1049`). Functionally together, but TDD red bars and per-task review were skipped. Raster is a stub. GPKG tests are Shapefile tests. |
| 7 Mgr + drop ADO + app default | ADO drop is correct. App `szTitle` bug. Optional `SMT_PG_DSN` is present and skipped when unset. |
| 8 SMF codec + docs | Codec call is correct. Commit also promotes the whole SMF tree. Docs mostly updated; DLL table stale. |
| C++20/23 | Orthogonal tree bump; `16f3cb9` mixed in unrelated TUs (including ADO recordset). |

Progress notes match the tree. Implementer was transparent about the missing drivers.

---

### Assessment

**Ready to merge?** **With fixes** (not ready to call the ADO→OGR replacement done).

**Reasoning:** The architectural cutover is real: `src_all` no longer links ADO, the manager constructs `OgrDataSource`, traits-based codec/layers exist, ACCESS is rejected, and SMF shares the codec. The advertised GeoPackage provider is not GeoPackage on this SDK (Shapefile fallback + app stem-only `szDBName`), the GDAL device DLL is empty, and style / Multi* / query / raster I/O are below the spec. Fix the GPKG-or-fail policy, the sample path, and the `ogr_codec` / `sde_gdal` split before treating this as the production DB path.
