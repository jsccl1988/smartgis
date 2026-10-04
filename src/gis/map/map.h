// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_MAP_H
#define _GIS_MAP_H

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

  bool AddLayer(MapLayer layer);
  bool AddLayer(OGRLayer* layer);

  bool DeleteLayer(const char* szName);
  bool DeleteLayer(OGRLayer* layer);

  bool MoveTo(int fromIndex, int toIndex);
  bool MoveToBottom(int index);
  bool MoveToTop(int index);

  void SetActiveLayer(const char* szName);
  void SetActiveOgrLayer(OGRLayer* layer);

  MapLayer* GetActiveMapLayer();
  const MapLayer* GetActiveMapLayer() const;
  OGRLayer* GetActiveOgrLayer();
  const OGRLayer* GetActiveOgrLayer() const;

  MapLayer* GetMapLayer(const char* szName);
  const MapLayer* GetMapLayer(const char* szName) const;
  MapLayer* GetMapLayer(int index);
  const MapLayer* GetMapLayer(int index) const;

  OGRLayer* GetOgrLayer(const char* szName);
  const OGRLayer* GetOgrLayer(const char* szName) const;
  OGRLayer* GetOgrLayer(int index);
  const OGRLayer* GetOgrLayer(int index) const;

  int GetLayerCount() const { return static_cast<int>(layers_.size()); }
  LayerType GetLayerType(int index) const;
  const char* GetLayerName(int index) const;
  bool IsLayerVisible(int index) const;
  void SetLayerVisible(int index, bool visible);

  virtual bool AppendFeature(OGRFeature* feature);
  bool AppendFeature(Feature* feature, bool clone = false);
  virtual bool DeleteFeature(OGRFeature* feature);
  virtual bool UpdateFeature(OGRFeature* feature);
  virtual bool QueryFeature(const GeomQueryDesc* gquery,
                            const AttrQueryDesc* pquery, OGRLayer* result,
                            int& n_geom_type);

  void MoveFirst() const;
  void MoveNext() const;
  void MoveLast() const;
  void Delete();
  bool IsEnd() const;
  void DeleteAll();

  void SetMapName(const char* szName) {
    strcpy_s(m_szMapName, k_map_name_max, szName);
  }
  const char* GetMapName() const { return m_szMapName; }

  void get_envelope(Envelope& env) const {
    memcpy(&env, &m_MapEnvelope, sizeof(Envelope));
  }
  void CalEnvelope();

 protected:
  int index_of_name(const char* szName) const;
  void envelope_of(const MapLayer& layer, Envelope* env) const;

  char m_szMapName[k_map_name_max];
  Envelope m_MapEnvelope;
  std::vector<MapLayer> layers_;
  int active_ = -1;
  mutable int m_nIteratorIndex = 0;
};

// Leftover 2010 name. Same type as Map (not a second document class).
using SmtMap = Map;

}  // namespace gis

#endif  // _GIS_MAP_H
