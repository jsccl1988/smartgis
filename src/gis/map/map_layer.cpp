// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/map/map_layer.h"

#include "gis/tile/layer/provider_tile_layer.h"
#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "ogrsf_frmts.h"

namespace gis {

MapLayer::~MapLayer() {
  if (owns_raster_) {
    delete raster_;
  }
  raster_ = nullptr;
  if (owns_tile_) {
    delete tile_;
  }
  tile_ = nullptr;
  if (owned_ds_) {
    GDALClose(owned_ds_);
    owned_ds_ = nullptr;
    ogr_ = nullptr;
    owns_ogr_ = false;
  }
  ogr_ = nullptr;
}

MapLayer::MapLayer(MapLayer&& other) noexcept
    : type_(other.type_),
      ogr_(other.ogr_),
      owns_ogr_(other.owns_ogr_),
      owned_ds_(other.owned_ds_),
      raster_(other.raster_),
      owns_raster_(other.owns_raster_),
      tile_(other.tile_),
      owns_tile_(other.owns_tile_),
      visible_(other.visible_),
      vector_schema_(other.vector_schema_),
      style_name_(std::move(other.style_name_)),
      style_document_(std::move(other.style_document_)) {
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
  other.owned_ds_ = nullptr;
  other.raster_ = nullptr;
  other.owns_raster_ = false;
  other.tile_ = nullptr;
  other.owns_tile_ = false;
}

MapLayer& MapLayer::operator=(MapLayer&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (owns_raster_) {
    delete raster_;
  }
  if (owns_tile_) {
    delete tile_;
  }
  if (owned_ds_) {
    GDALClose(owned_ds_);
  }
  type_ = other.type_;
  ogr_ = other.ogr_;
  owns_ogr_ = other.owns_ogr_;
  owned_ds_ = other.owned_ds_;
  raster_ = other.raster_;
  owns_raster_ = other.owns_raster_;
  tile_ = other.tile_;
  owns_tile_ = other.owns_tile_;
  visible_ = other.visible_;
  vector_schema_ = other.vector_schema_;
  style_name_ = std::move(other.style_name_);
  style_document_ = std::move(other.style_document_);
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
  other.owned_ds_ = nullptr;
  other.raster_ = nullptr;
  other.owns_raster_ = false;
  other.tile_ = nullptr;
  other.owns_tile_ = false;
  return *this;
}

MapLayer MapLayer::from_ogr(OGRLayer* layer) {
  MapLayer m;
  m.type_ = LYR_VECTOR;
  m.ogr_ = layer;
  m.owns_ogr_ = false;
  return m;
}

MapLayer MapLayer::adopt_dataset(GDALDataset* ds, OGRLayer* layer) {
  MapLayer m;
  m.type_ = LYR_VECTOR;
  m.owned_ds_ = ds;
  m.ogr_ = layer;
  m.owns_ogr_ = ds != nullptr;
  return m;
}

MapLayer MapLayer::from_raster(datasource::OgrRasterLayer* raster, bool owns) {
  MapLayer m;
  m.raster_ = raster;
  m.owns_raster_ = owns && raster != nullptr;
  m.type_ = LYR_RASTER;
  return m;
}

MapLayer MapLayer::from_tile(tile::ProviderTileLayer* tile, bool owns) {
  MapLayer m;
  m.tile_ = tile;
  m.owns_tile_ = owns && tile != nullptr;
  m.type_ = LYR_TITLE;
  return m;
}

const char* MapLayer::name() const {
  if (ogr_) {
    return ogr_->GetName();
  }
  if (raster_) {
    return raster_->GetLayerName();
  }
  if (tile_) {
    return tile_->GetLayerName();
  }
  return "";
}

bool MapLayer::visible() const { return visible_; }

void MapLayer::set_visible(bool visible) { visible_ = visible; }

void MapLayer::set_style_name(const char* name) {
  style_name_ = name ? name : "";
}

void MapLayer::set_style_document(std::shared_ptr<style::StyleDocument> doc) {
  style_document_ = std::move(doc);
}

void MapLayer::get_envelope(gis::Envelope* out) const {
  if (!out) {
    return;
  }
  *out = gis::Envelope();
  if (raster_) {
    raster_->get_envelope(*out);
    return;
  }
  if (tile_) {
    tile_->get_envelope(*out);
    return;
  }
  if (!ogr_) {
    return;
  }
  OGREnvelope ogr_env;
  if (ogr_->GetExtent(&ogr_env, TRUE) == OGRERR_NONE) {
    out->MinX = ogr_env.MinX;
    out->MinY = ogr_env.MinY;
    out->MaxX = ogr_env.MaxX;
    out->MaxY = ogr_env.MaxY;
  }
}

void MapLayer::cal_envelope() {
  if (raster_) {
    raster_->CalEnvelope();
  }
  if (tile_) {
    tile_->CalEnvelope();
  }
}

OGRFeatureDefn* MapLayer::layer_defn() {
  return ogr_ ? ogr_->GetLayerDefn() : nullptr;
}

const OGRFeatureDefn* MapLayer::layer_defn() const {
  return ogr_ ? ogr_->GetLayerDefn() : nullptr;
}

void MapLayer::reset_reading() {
  if (ogr_) {
    ogr_->ResetReading();
  }
}

OGRwkbGeometryType MapLayer::geometry_type() const {
  if (ogr_) {
    return ogr_->GetGeomType();
  }
  return wkbNone;
}

Feature MapLayer::next_feature() {
  if (!ogr_) {
    return Feature();
  }
  return Feature(ogr_->GetNextFeature(), true);
}

void MapLayer::set_spatial_filter(OGRGeometry* geom) {
  if (ogr_) {
    ogr_->SetSpatialFilter(geom);
  }
}

void MapLayer::set_spatial_filter_rect(double minx, double miny, double maxx,
                                       double maxy) {
  if (ogr_) {
    ogr_->SetSpatialFilterRect(minx, miny, maxx, maxy);
  }
}

void MapLayer::clear_spatial_filter() { set_spatial_filter(nullptr); }

OGRGeometry* MapLayer::spatial_filter() {
  return ogr_ ? ogr_->GetSpatialFilter() : nullptr;
}

const OGRGeometry* MapLayer::spatial_filter() const {
  return ogr_ ? ogr_->GetSpatialFilter() : nullptr;
}

bool MapLayer::set_attribute_filter(const char* query) {
  if (!ogr_) {
    return false;
  }
  return ogr_->SetAttributeFilter(query) == OGRERR_NONE;
}

void MapLayer::clear_attribute_filter() {
  if (ogr_) {
    ogr_->SetAttributeFilter(nullptr);
  }
}

OGRSpatialReference* MapLayer::spatial_ref() {
  return ogr_ ? ogr_->GetSpatialRef() : nullptr;
}

const OGRSpatialReference* MapLayer::spatial_ref() const {
  return ogr_ ? ogr_->GetSpatialRef() : nullptr;
}

const char* MapLayer::fid_column() const {
  return ogr_ ? ogr_->GetFIDColumn() : "";
}

}  // namespace gis
