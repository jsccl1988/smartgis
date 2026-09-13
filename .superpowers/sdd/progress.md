# SDD progress — OGR database datasource

Plan: `docs/superpowers/plans/2026-09-13-ogr-db-datasource.md`
Spec: `docs/superpowers/specs/2026-09-13-ogr-db-datasource-design.md` (status: accepted)

Branch: `master` (no topic branch)

## Completed tasks

| Task | Status | SHA | Subject |
| --- | --- | --- | --- |
| 1 | complete | `c224770f8a08ba4a29b09612c5c08ca30f20c044` | Add OGR DB provider ids and GDAL open-target helper. |
| 2 | complete | `c0f56138d9cdddaf47bf1108bc869ddd8f9ac7f5` | Add shared OGR feature codec for point, line, polygon, and annotation. |
| 3–6 | complete | `0ee104970bd3e2a66d867be573de4701b1c7929a` | Open GeoPackage datasets through GDAL and round-trip OGR layers. |
| 7 | complete | `a22baafc7096e1e4acb415f3a897461dfe8ec1dc` | Route database datasources through OGR and drop ADO from src_all. |
| 8 | complete | `c1d4774092bc0e0f368e1c74fd6a26a681565774` | Share the OGR feature codec with SMF and document the GDAL DB provider. |
| review | complete | `b15d04bdccec7cf8c3c87d6d49c510614098b3c3` | Honest GPKG handling, sde_gdal DLL, sample path, delete leftover ADO. |

## Test summary

- `out\sde_gdal_test.exe` — exit 0, `sde_gdal_test connect checks ok`
- Memory-driver: anno + style OFTBinary + MultiPoint/MultiLineString/MultiPolygon
- This SDK has no GPKG driver: Create/Open fail and print `SKIP` (not Shapefile)
- `gn desc out //src:src_all deps --all` has `sde_gdal` and no `ado` / `sde_ado`

## Deleted vs kept

Deleted: `src/ado`, `src/sdb/datasource/ado`, `src/SmtAdoCore`, `src/SmtSDEAdoDevice` (nothing in src_all/mgr/plugin/app linked them). Dead SMF `CopyOGRGeomToSmtGeom` / `CopyOGRAttToSmtAtt`. Shapefile-as-GPKG fallback.

Kept: `ogr_codec` (SMF + tests still use connect/codec) and `sde_gdal` (dataset/layers). SMF type-map helpers used by `smfveclayer.cpp`.
