// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SESSION_DATASET_HANDLE_H_
#define GIS_DATASOURCE_SESSION_DATASET_HANDLE_H_

#include "gdal_priv.h"
#include "gis/gis_export.h"
#include "gis/map/map_layer.h"

namespace gis {
namespace datasource {

// RAII owned GDALDataset. Product lists layers as MapLayer (does not own OGR).
class GIS_EXPORT DatasetHandle {
 public:
  DatasetHandle() = default;
  explicit DatasetHandle(GDALDataset* owned);
  ~DatasetHandle();

  DatasetHandle(DatasetHandle&& other) noexcept;
  DatasetHandle& operator=(DatasetHandle&& other) noexcept;
  DatasetHandle(const DatasetHandle&) = delete;
  DatasetHandle& operator=(const DatasetHandle&) = delete;

  explicit operator bool() const;
  GDALDataset* gdal() const;  // non-owning view
  GDALDataset* release();     // give up ownership

  int layer_count() const;
  MapLayer layer_at(int index) const;  // MapLayer::from_ogr
  MapLayer layer_by_name(const char* name) const;

 private:
  GDALDataset* ds_ = nullptr;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SESSION_DATASET_HANDLE_H_
