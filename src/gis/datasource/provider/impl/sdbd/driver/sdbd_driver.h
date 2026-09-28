// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_DRIVER_SDBD_DRIVER_H_
#define GIS_DATASOURCE_SDBD_DRIVER_SDBD_DRIVER_H_

#include <cstddef>
#include <string>
#include <string_view>

#include "gis/gis_export.h"

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

namespace detail {

inline bool equals_ci(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    unsigned char ca = static_cast<unsigned char>(a[i]);
    unsigned char cb = static_cast<unsigned char>(b[i]);
    if (ca >= 'A' && ca <= 'Z') {
      ca = static_cast<unsigned char>(ca - 'A' + 'a');
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = static_cast<unsigned char>(cb - 'A' + 'a');
    }
    if (ca != cb) {
      return false;
    }
  }
  return true;
}

// Empty, MEM/Memory, or the decorator name itself all mean an inner Memory dataset.
inline bool is_memory_open_driver(std::string_view driver) {
  return driver.empty() || equals_ci(driver, "mem") ||
         equals_ci(driver, "memory") || equals_ci(driver, kSdbdDriverName);
}

}  // namespace detail

// Single "SDBD:<driver>:<target>" spelling. Memory drivers normalize to MEM
// and an empty target becomes "sdbd".
inline std::string format_sdbd_open_name(std::string_view driver,
                                         std::string_view target) {
  std::string name(kSdbdPrefix);
  if (detail::is_memory_open_driver(driver)) {
    name += "MEM:";
    if (target.empty()) {
      name += "sdbd";
    } else {
      name.append(target);
    }
    return name;
  }
  name.append(driver);
  name += ':';
  name.append(target);
  return name;
}

// Registers the SDBD driver with GDALDriverManager. Idempotent.
GIS_EXPORT bool register_sdbd_driver();

GIS_EXPORT SdbdDataset* as_sdbd_dataset(GDALDataset* ds);
GIS_EXPORT SdbdLayer* as_sdbd_layer(OGRLayer* layer);

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_DRIVER_SDBD_DRIVER_H_
