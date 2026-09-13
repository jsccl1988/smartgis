### Task 2: Bidirectional feature codec (point, line, polygon, annotation)

**Files:**
- Create: `src/sdb/datasource/gdal/ogr_feature_kind.h`
- Create: `src/sdb/datasource/gdal/ogr_feature_codec.h`
- Create: `src/sdb/datasource/gdal/ogr_feature_codec.cc`
- Modify: `src/sdb/datasource/gdal/BUILD.gn` (`ogr_codec` sources + `//third_party:gdal`)
- Modify: `src/sdb/datasource/gdal/sde_gdal_test.cc`

**Interfaces:**
- Consumes: `OGRFeature`, `SmtFeature`, `SmtPoint` / `SmtLineString` / `SmtPolygon` / `SmtLinearRing`
- Produces:
  - `sdb::datasource::copy_ogr_feature_to_smt(OGRFeature* src, SmtFeature* dst) -> bool`
  - `sdb::datasource::copy_smt_feature_to_ogr(const SmtFeature* src, OGRFeature* dst) -> bool`
  - Annotation fields: `anno` (string), `color` (int), `angle` (real)
  - `feature_kind_traits<SmtFeatureType>` + `visit_feature_kind` (header-only)

**SDK note:** This tree’s GDAL is 3.9. Memory-driver tests use `GDALDriver` / `GDALDataset::Create` (not deprecated `OGRDataSource::CreateDataSource`).

- [ ] **Step 1: Write the failing test** (no codec sources yet)
- [ ] **Step 2: Run test to verify it fails** (`copy_smt_feature_to_ogr` missing)
- [ ] **Step 3: Write minimal implementation**
- [ ] **Step 4: Run the tests and make sure they pass**
- [ ] **Step 5: Commit**
