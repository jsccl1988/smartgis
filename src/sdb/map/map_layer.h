// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_MAP_MAP_LAYER_H_
#define SDB_MAP_MAP_LAYER_H_

#include "base/style/envelope.h"
#include "sdb/feature/feature.h"
#include "sdb/gis_export.h"
#include "sdb/layer/layer.h"
#include "sdb/style/style_types.h"

#include <memory>
#include <string>

class GDALDataset;
class OGRLayer;

namespace sdb {

// One map document layer: composes OGRLayer (or leftover raster/tile) plus
// product metadata. Does not subclass OGR.
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
  static MapLayer from_leftover(SmtLayer* layer, bool owns = true);

  OGRLayer* ogr() { return ogr_; }
  const OGRLayer* ogr() const { return ogr_; }
  bool owns_ogr() const { return owns_ogr_; }
  GDALDataset* owned_dataset() { return owned_ds_; }

  SmtLayer* leftover() { return leftover_; }
  const SmtLayer* leftover() const { return leftover_; }

  SmtLayerType layer_type() const { return type_; }
  void set_layer_type(SmtLayerType type) { type_ = type; }

  const char* name() const;
  bool visible() const;
  void set_visible(bool visible);

  SmtFeatureType feature_type() const { return feature_type_; }
  void set_feature_type(SmtFeatureType ft) { feature_type_ = ft; }

  const std::string& style_name() const { return style_name_; }
  void set_style_name(const char* name);

  // Optional MapLibre-subset document (sdb::style). Independent of style_name_.
  const std::shared_ptr<style::StyleDocument>& style_document() const {
    return style_document_;
  }
  void set_style_document(std::shared_ptr<style::StyleDocument> doc);

  void get_envelope(base::Envelope* out) const;
  void cal_envelope();

 private:
  SmtLayerType type_ = LYR_VECTOR;
  OGRLayer* ogr_ = nullptr;
  bool owns_ogr_ = false;
  GDALDataset* owned_ds_ = nullptr;
  SmtLayer* leftover_ = nullptr;
  bool owns_leftover_ = true;
  bool visible_ = true;
  SmtFeatureType feature_type_ = SmtFtUnknown;
  std::string style_name_;
  std::shared_ptr<style::StyleDocument> style_document_;
};

}  // namespace sdb

#endif  // SDB_MAP_MAP_LAYER_H_
