// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/session/dataset_handle.h"

namespace gis {
namespace datasource {

DatasetHandle::DatasetHandle(GDALDataset* owned) : ds_(owned) {}

DatasetHandle::~DatasetHandle() {
  if (ds_) {
    GDALClose(ds_);
    ds_ = nullptr;
  }
}

DatasetHandle::DatasetHandle(DatasetHandle&& other) noexcept : ds_(other.ds_) {
  other.ds_ = nullptr;
}

DatasetHandle& DatasetHandle::operator=(DatasetHandle&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (ds_) {
    GDALClose(ds_);
  }
  ds_ = other.ds_;
  other.ds_ = nullptr;
  return *this;
}

DatasetHandle::operator bool() const {
  return ds_ != nullptr;
}

GDALDataset* DatasetHandle::gdal() const {
  return ds_;
}

GDALDataset* DatasetHandle::release() {
  GDALDataset* out = ds_;
  ds_ = nullptr;
  return out;
}

int DatasetHandle::layer_count() const {
  return ds_ ? ds_->GetLayerCount() : 0;
}

MapLayer DatasetHandle::layer_at(int index) const {
  if (!ds_ || index < 0 || index >= ds_->GetLayerCount()) {
    return MapLayer();
  }
  return MapLayer::from_ogr(ds_->GetLayer(index));
}

MapLayer DatasetHandle::layer_by_name(const char* name) const {
  if (!ds_ || !name) {
    return MapLayer();
  }
  return MapLayer::from_ogr(ds_->GetLayerByName(name));
}

}  // namespace datasource
}  // namespace gis
