// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_MAP_MAP_LAYER_H_
#define SDB_MAP_MAP_LAYER_H_

#include <memory>
#include <string>

#include "gis/envelope.h"
#include "gis/feature/feature.h"
#include "gis/gis_export.h"
#include "gis/map/layer_kind.h"
#include "gis/style/style_types.h"
#include "ogr_core.h"

class GDALDataset;
class OGRFeatureDefn;
class OGRGeometry;
class OGRLayer;
class OGRSpatialReference;

namespace gis {
namespace datasource {
class OgrRasterLayer;
}
namespace tile {
class ProviderTileLayer;
}

// One map document slot: OGR vector and/or product raster/tile. Not leftover
// Layer. Leftover wraps these product pointers under leftover/gis/layer/.
class GIS_EXPORT MapLayer {
 public:
  MapLayer() = default;
  ~MapLayer();

  MapLayer(MapLayer&& other) noexcept;
  MapLayer& operator=(MapLayer&& other) noexcept;
  MapLayer(const MapLayer&) = delete;
  MapLayer& operator=(const MapLayer&) = delete;

  static MapLayer from_ogr(OGRLayer* layer);
  static MapLayer adopt_dataset(GDALDataset* ds, OGRLayer* layer);
  static MapLayer from_raster(datasource::OgrRasterLayer* raster, bool owns);
  static MapLayer from_tile(tile::ProviderTileLayer* tile, bool owns);

  OGRLayer* ogr() { return ogr_; }
  const OGRLayer* ogr() const { return ogr_; }
  bool owns_ogr() const { return owns_ogr_; }
  GDALDataset* owned_dataset() { return owned_ds_; }

  datasource::OgrRasterLayer* raster() { return raster_; }
  const datasource::OgrRasterLayer* raster() const { return raster_; }
  tile::ProviderTileLayer* tile() { return tile_; }
  const tile::ProviderTileLayer* tile() const { return tile_; }

  LayerType layer_type() const { return type_; }
  void set_layer_type(LayerType type) { type_ = type; }

  const char* name() const;
  bool visible() const;
  void set_visible(bool visible);

  OGRwkbGeometryType geometry_type() const;
  gis::VectorSchema vector_schema() const { return vector_schema_; }
  void set_vector_schema(gis::VectorSchema schema) { vector_schema_ = schema; }

  const std::string& style_name() const { return style_name_; }
  void set_style_name(const char* name);

  const std::shared_ptr<style::StyleDocument>& style_document() const {
    return style_document_;
  }
  void set_style_document(std::shared_ptr<style::StyleDocument> doc);

  void get_envelope(gis::Envelope* out) const;
  void cal_envelope();

  OGRFeatureDefn* layer_defn();
  const OGRFeatureDefn* layer_defn() const;

  void reset_reading();
  Feature next_feature();

  void set_spatial_filter(OGRGeometry* geom);
  void set_spatial_filter_rect(double minx, double miny, double maxx,
                               double maxy);
  void clear_spatial_filter();
  OGRGeometry* spatial_filter();
  const OGRGeometry* spatial_filter() const;

  bool set_attribute_filter(const char* query);
  void clear_attribute_filter();

  OGRSpatialReference* spatial_ref();
  const OGRSpatialReference* spatial_ref() const;

  const char* fid_column() const;

 private:
  LayerType type_ = LYR_VECTOR;
  OGRLayer* ogr_ = nullptr;
  bool owns_ogr_ = false;
  GDALDataset* owned_ds_ = nullptr;
  datasource::OgrRasterLayer* raster_ = nullptr;
  bool owns_raster_ = false;
  tile::ProviderTileLayer* tile_ = nullptr;
  bool owns_tile_ = false;
  bool visible_ = true;
  gis::VectorSchema vector_schema_ = gis::VectorSchema::kNone;
  std::string style_name_;
  std::shared_ptr<style::StyleDocument> style_document_;
};

}  // namespace gis

#endif  // SDB_MAP_MAP_LAYER_H_
