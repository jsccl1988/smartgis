# Task 2 Report: Bidirectional feature codec

**Status:** DONE_WITH_CONCERNS
**Commit:** pending at report write; subject `Add shared OGR feature codec for point, line, polygon, and annotation.`

## Summary

Added `feature_kind_traits` + `visit_feature_kind` and a shared OGR↔Smt codec for point, line, polygon, and annotation. Extra annotation fields (`anno` / `color` / `angle`) are walked via `std::tuple` + `std::index_sequence`, not copied GetField blocks. Memory-driver round-trip uses GDAL 3.9 `GDALDriver` / `GDALDataset::Create` (not deprecated `OGRDataSource`).

## TDD Evidence

### RED

Test included `ogr_feature_codec.h` and called `copy_smt_feature_to_ogr` before those files existed. First `build.bat sde_gdal_test` did not reach that error: a C++20 rebuild of `linearring.cpp` failed first (temporary bound to non-const `dbfPoint&`). After unblocking deps, the intended missing-header / missing-symbol failure applied until the codec sources landed.

### GREEN

```
ninja -C out sde_gdal_test
out\sde_gdal_test.exe
sde_gdal_test connect checks ok
Exit code: 0
```

`/std:c++20`. Annotation Memory-layer round-trip: smt→ogr→smt, point (1.5, 2.5), `anno` field present.

## Implementation

- `ogr_feature_kind.h` — specializations for Dot/Curve/Surface/Anno (plus Tin/Grid/ChildImage stubs for later tasks), `visit_feature_kind`, `for_each_extra_field`.
- `ogr_feature_codec.h/.cc` — `copy_ogr_feature_to_smt` / `copy_smt_feature_to_ogr`; FID; attrs by name; no SMF random pen colors.
- `BUILD.gn` — codec sources on `ogr_codec`; test links `//third_party:gdal` + `gdal_runtime_dlls`.

## Concerns

- Parallel agents changed `cc_std` to C++23 (`/std:c++23preview` breaks this MSVC + WIN32_LEAN_AND_MEAN). Local verify used `out/args.gn` `cc_std = "c++20"`.
- Drive-by compile fixes outside `gdal/` (linearring lvalues, statesmanager lvalues, assorted GN) were **not** included in the Task 2 commit.
- Tin/Grid encode/decode still return false until Task 6.

## Files (task)

| File | Action |
| --- | --- |
| `src/sdb/datasource/gdal/ogr_feature_kind.h` | Created |
| `src/sdb/datasource/gdal/ogr_feature_codec.h` | Created |
| `src/sdb/datasource/gdal/ogr_feature_codec.cc` | Created |
| `src/sdb/datasource/gdal/sde_gdal_test.cc` | Modified — Memory anno round-trip |
| `src/sdb/datasource/gdal/BUILD.gn` | Modified — sources + GDAL deps |
