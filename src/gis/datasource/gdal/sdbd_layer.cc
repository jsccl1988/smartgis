// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/sdbd_layer.h"

#include <cstdio>

#include "gis/datasource/gdal/sdbd_dataset.h"
#include "gis/datasource/gdal/sdbd_gdal_driver.h"

namespace gis {
namespace datasource {

SdbdLayer::SdbdLayer(OGRLayer* inner, gis::SmtFeatureType ft,
                     SdbdDataset* owner)
    : inner_(inner), owner_(owner), feature_type_(ft) {
  write_metadata();
}

void SdbdLayer::set_feature_type(gis::SmtFeatureType ft) {
  feature_type_ = ft;
  write_metadata();
}

void SdbdLayer::set_style_hint(const char* name) {
  style_hint_ = name ? name : "";
  write_metadata();
}

void SdbdLayer::write_metadata() {
  if (!inner_) {
    return;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(feature_type_));
  inner_->SetMetadataItem(kSdbdMetaFeatureType, buf, kSdbdMetadataDomain);
  if (!style_hint_.empty()) {
    inner_->SetMetadataItem(kSdbdMetaStyleHint, style_hint_.c_str(),
                            kSdbdMetadataDomain);
  }
}

const char* SdbdLayer::GetName() { return inner_ ? inner_->GetName() : ""; }

OGRwkbGeometryType SdbdLayer::GetGeomType() {
  return inner_ ? inner_->GetGeomType() : wkbUnknown;
}

OGRFeatureDefn* SdbdLayer::GetLayerDefn() {
  return inner_ ? inner_->GetLayerDefn() : nullptr;
}

void SdbdLayer::ResetReading() {
  if (inner_) {
    inner_->ResetReading();
  }
}

OGRFeature* SdbdLayer::GetNextFeature() {
  return inner_ ? inner_->GetNextFeature() : nullptr;
}

OGRFeature* SdbdLayer::GetFeature(GIntBig fid) {
  return inner_ ? inner_->GetFeature(fid) : nullptr;
}

GIntBig SdbdLayer::GetFeatureCount(int force) {
  return inner_ ? inner_->GetFeatureCount(force) : 0;
}

OGRErr SdbdLayer::SetNextByIndex(GIntBig index) {
  return inner_ ? inner_->SetNextByIndex(index) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::ISetFeature(OGRFeature* feature) {
  return inner_ ? inner_->SetFeature(feature) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::ICreateFeature(OGRFeature* feature) {
  return inner_ ? inner_->CreateFeature(feature) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::DeleteFeature(GIntBig fid) {
  return inner_ ? inner_->DeleteFeature(fid) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::CreateField(const OGRFieldDefn* defn, int approx) {
  return inner_ ? inner_->CreateField(defn, approx) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::DeleteField(int index) {
  return inner_ ? inner_->DeleteField(index) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::ReorderFields(int* map) {
  return inner_ ? inner_->ReorderFields(map) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::AlterFieldDefn(int index, OGRFieldDefn* defn, int flags) {
  return inner_ ? inner_->AlterFieldDefn(index, defn, flags) : OGRERR_FAILURE;
}

OGRSpatialReference* SdbdLayer::GetSpatialRef() {
  return inner_ ? inner_->GetSpatialRef() : nullptr;
}

OGRErr SdbdLayer::GetExtent(OGREnvelope* env, int force) {
  return inner_ ? inner_->GetExtent(env, force) : OGRERR_FAILURE;
}

OGRErr SdbdLayer::GetExtent(int geom, OGREnvelope* env, int force) {
  return inner_ ? inner_->GetExtent(geom, env, force) : OGRERR_FAILURE;
}

int SdbdLayer::TestCapability(const char* cap) {
  return inner_ ? inner_->TestCapability(cap) : FALSE;
}

OGRErr SdbdLayer::SetAttributeFilter(const char* filter) {
  return inner_ ? inner_->SetAttributeFilter(filter) : OGRERR_FAILURE;
}

void SdbdLayer::SetSpatialFilter(OGRGeometry* geom) {
  if (inner_) {
    inner_->SetSpatialFilter(geom);
  }
}

void SdbdLayer::SetSpatialFilter(int geom, OGRGeometry* filter) {
  if (inner_) {
    inner_->SetSpatialFilter(geom, filter);
  }
}

void SdbdLayer::SetSpatialFilterRect(double min_x, double min_y, double max_x,
                                     double max_y) {
  if (inner_) {
    inner_->SetSpatialFilterRect(min_x, min_y, max_x, max_y);
  }
}

void SdbdLayer::SetSpatialFilterRect(int geom, double min_x, double min_y,
                                     double max_x, double max_y) {
  if (inner_) {
    inner_->SetSpatialFilterRect(geom, min_x, min_y, max_x, max_y);
  }
}

OGRGeometry* SdbdLayer::GetSpatialFilter() {
  return inner_ ? inner_->GetSpatialFilter() : nullptr;
}

const char* SdbdLayer::GetFIDColumn() {
  return inner_ ? inner_->GetFIDColumn() : "";
}

const char* SdbdLayer::GetGeometryColumn() {
  return inner_ ? inner_->GetGeometryColumn() : "";
}

OGRErr SdbdLayer::StartTransaction() {
  return inner_ ? inner_->StartTransaction() : OGRERR_FAILURE;
}

OGRErr SdbdLayer::CommitTransaction() {
  return inner_ ? inner_->CommitTransaction() : OGRERR_FAILURE;
}

OGRErr SdbdLayer::RollbackTransaction() {
  return inner_ ? inner_->RollbackTransaction() : OGRERR_FAILURE;
}

OGRErr SdbdLayer::SyncToDisk() {
  return inner_ ? inner_->SyncToDisk() : OGRERR_FAILURE;
}

GDALDataset* SdbdLayer::GetDataset() {
  if (owner_) {
    return owner_;
  }
  return inner_ ? inner_->GetDataset() : nullptr;
}

}  // namespace datasource
}  // namespace gis
