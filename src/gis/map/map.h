// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_MAP_MAP_H_
#define GIS_MAP_MAP_H_

#include <cstring>
#include <vector>

#include "gis/envelope.h"
#include "gis/feature/feature.h"
#include "gis/gis_export.h"
#include "gis/map/layer_kind.h"
#include "gis/map/map_layer.h"
#include "gis/map/query.h"

class OGRFeature;
class OGRLayer;

namespace gis {

inline constexpr int k_map_name_max = 50;

// Map document. Vector layers are MapLayer(OGRLayer*); product raster/tile
// hang on MapLayer::raster() / tile(). Leftover wraps those in leftover/.
class GIS_EXPORT Map {
 public:
  Map();
  virtual ~Map();
  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;
  Map(Map&&) noexcept = default;
  Map& operator=(Map&&) noexcept = default;

  bool add_layer(MapLayer layer);
  bool add_layer(OGRLayer* layer);

  bool delete_layer(const char* name);
  bool delete_layer(OGRLayer* layer);

  bool move_to(int from_index, int to_index);
  bool move_to_bottom(int index);
  bool move_to_top(int index);

  void set_active_layer(const char* name);
  void set_active_ogr_layer(OGRLayer* layer);

  MapLayer* active_map_layer();
  const MapLayer* active_map_layer() const;
  OGRLayer* active_ogr_layer();
  const OGRLayer* active_ogr_layer() const;

  MapLayer* map_layer(const char* name);
  const MapLayer* map_layer(const char* name) const;
  MapLayer* map_layer(int index);
  const MapLayer* map_layer(int index) const;

  OGRLayer* ogr_layer(const char* name);
  const OGRLayer* ogr_layer(const char* name) const;
  OGRLayer* ogr_layer(int index);
  const OGRLayer* ogr_layer(int index) const;

  int layer_count() const { return static_cast<int>(layers_.size()); }
  LayerType layer_type(int index) const;
  const char* layer_name(int index) const;
  bool is_layer_visible(int index) const;
  void set_layer_visible(int index, bool visible);

  virtual bool append_feature(OGRFeature* feature);
  bool append_feature(Feature* feature, bool clone = false);
  virtual bool delete_feature(OGRFeature* feature);
  virtual bool update_feature(OGRFeature* feature);
  virtual bool query_feature(const GeomQueryDesc* gquery,
                             const AttrQueryDesc* aquery, OGRLayer* result,
                             int& geom_type);

  void move_first() const;
  void move_next() const;
  void move_last() const;
  void erase();
  bool is_end() const;
  void clear();

  void set_name(const char* name) {
    strcpy_s(name_, k_map_name_max, name);
  }
  const char* name() const { return name_; }

  void get_envelope(Envelope& env) const {
    memcpy(&env, &envelope_, sizeof(Envelope));
  }
  void cal_envelope();

 protected:
  int index_of_name(const char* name) const;
  void envelope_of(const MapLayer& layer, Envelope* env) const;

  char name_[k_map_name_max];
  Envelope envelope_;
  std::vector<MapLayer> layers_;
  int active_ = -1;
  mutable int iterator_index_ = 0;
};

// Leftover 2010 name. Same type as Map (not a second document class).
using Map = Map;

}  // namespace gis

#endif  // GIS_MAP_MAP_H_
