// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_
#define SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_

#include "gis/datasource/gdal/ogr_export.h"

class GDALDataset;
class OGRLayer;

// Decorator driver (inherit + composition). Do not patch third_party/gdal
// and do not subclass GPKG / PostgreSQL / Memory internals.
//
// 1. GDALDriver "SDBD" (pfnIdentify / pfnOpen / pfnCreate).
// 2. SdbdDataset : GDALDataset owns a stock GDALDataset* inner_ from
//    GDALOpenEx / Create on Memory, GPKG, PostgreSQL, or file. Forward
//    GetLayerCount / raster / CreateLayer; wrap layers as SdbdLayer.
// 3. SdbdLayer : OGRLayer wraps OGRLayer* inner_ only for extras
//    (SmtFeatureType, style hint) plus SetMetadataItem(..., "SDBD").
// 4. Do not subclass OGRFeature — extras are OGR fields or style string.
// 5. Callers: GDALOpenEx("SDBD:…") / GetDriverByName("SDBD"), then
//    dynamic_cast to SdbdDataset / SdbdLayer. No OgrDataSource facade.

namespace gis {
namespace datasource {

class SdbdDataset;
class SdbdLayer;

inline constexpr char kSdbdDriverName[] = "SDBD";
inline constexpr char kSdbdPrefix[] = "SDBD:";
inline constexpr char kSdbdMetadataDomain[] = "SDBD";
inline constexpr char kSdbdMetaFeatureType[] = "SMT_FEATURE_TYPE";
inline constexpr char kSdbdMetaStyleHint[] = "SMT_STYLE_HINT";

// Registers the SDBD driver with GDALDriverManager. Idempotent.
SDE_GDAL_EXPORT bool register_sdbd_driver();

SDE_GDAL_EXPORT SdbdDataset* as_sdbd_dataset(GDALDataset* ds);
SDE_GDAL_EXPORT SdbdLayer* as_sdbd_layer(OGRLayer* layer);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_SDBD_GDAL_DRIVER_H_
