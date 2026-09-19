// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_MAP_H
#define _GIS_MAP_H

#include <vector>

#include "base/carto/envelope.h"
#include "gis/feature/feature.h"
#include "gis/gis_export.h"
#include "gis/layer/layer.h"
#include "gis/map/map_layer.h"

class OGRFeature;
class OGRLayer;

using namespace base;

#define MAX_MAP_NAME MAX_NAME_LENGTH

namespace gis {

// Map document. Vector layers are MapLayer(OGRLayer*); raster/tile leftovers
// live on MapLayer::leftover().
class GIS_EXPORT SmtMap {
 public:
  SmtMap();
  virtual ~SmtMap();
  SmtMap(const SmtMap&) = delete;
  SmtMap& operator=(const SmtMap&) = delete;
  SmtMap(SmtMap&&) noexcept = default;
  SmtMap& operator=(SmtMap&&) noexcept = default;

  bool AddLayer(MapLayer layer);
  bool AddLayer(OGRLayer* layer);
  bool AddLayer(SmtLayer* layer);

  bool DeleteLayer(const char* szName);
  bool DeleteLayer(OGRLayer* layer);
  bool DeleteLayer(const SmtLayer* layer);

  bool MoveTo(int fromIndex, int toIndex);
  bool MoveToBottom(int index);
  bool MoveToTop(int index);

  void SetActiveLayer(const char* szName);
  void SetActiveOgrLayer(OGRLayer* layer);

  MapLayer* GetActiveMapLayer();
  const MapLayer* GetActiveMapLayer() const;
  OGRLayer* GetActiveOgrLayer();
  const OGRLayer* GetActiveOgrLayer() const;
  SmtLayer* GetActiveLeftoverLayer();
  const SmtLayer* GetActiveLeftoverLayer() const;

  SmtLayer* GetActiveLayer() { return GetActiveLeftoverLayer(); }
  const SmtLayer* GetActiveLayer() const { return GetActiveLeftoverLayer(); }
  SmtLayer* GetLayer(const char* szName) { return GetLeftoverLayer(szName); }
  const SmtLayer* GetLayer(const char* szName) const {
    return GetLeftoverLayer(szName);
  }
  SmtLayer* GetLayer(int index) { return GetLeftoverLayer(index); }
  const SmtLayer* GetLayer(int index) const { return GetLeftoverLayer(index); }
  // Iterator current leftover (null for OGR-only layers). Prefer GetMapLayer /
  // GetLayerName while walking MoveFirst/MoveNext.
  SmtLayer* GetLayer();
  bool AddLayer(const SmtLayer* layer) {
    return AddLayer(const_cast<SmtLayer*>(layer));
  }

  MapLayer* GetMapLayer(const char* szName);
  const MapLayer* GetMapLayer(const char* szName) const;
  MapLayer* GetMapLayer(int index);
  const MapLayer* GetMapLayer(int index) const;

  OGRLayer* GetOgrLayer(const char* szName);
  const OGRLayer* GetOgrLayer(const char* szName) const;
  SmtLayer* GetLeftoverLayer(const char* szName);
  const SmtLayer* GetLeftoverLayer(const char* szName) const;

  int GetLayerCount() const { return static_cast<int>(layers_.size()); }
  SmtLayerType GetLayerType(int index) const;
  const char* GetLayerName(int index) const;
  bool IsLayerVisible(int index) const;
  void SetLayerVisible(int index, bool visible);

  OGRLayer* GetOgrLayer(int index);
  const OGRLayer* GetOgrLayer(int index) const;
  SmtLayer* GetLeftoverLayer(int index);
  const SmtLayer* GetLeftoverLayer(int index) const;

  virtual bool AppendFeature(OGRFeature* feature);
  bool AppendFeature(SmtFeature* feature, bool clone = false);
  virtual bool DeleteFeature(OGRFeature* feature);
  virtual bool UpdateFeature(OGRFeature* feature);
  virtual bool QueryFeature(const SmtGQueryDesc* gquery,
                            const SmtPQueryDesc* pquery, OGRLayer* result,
                            int& nFeaType);

  void MoveFirst() const;
  void MoveNext() const;
  void MoveLast() const;
  void Delete();
  bool IsEnd() const;
  void DeleteAll();

  void SetMapName(const char* szName) {
    strcpy_s(m_szMapName, MAX_MAP_NAME, szName);
  }
  const char* GetMapName() const { return m_szMapName; }

  void get_envelope(Envelope& env) const {
    memcpy(&env, &m_MapEnvelope, sizeof(Envelope));
  }
  void CalEnvelope();

 protected:
  int index_of_name(const char* szName) const;
  void envelope_of(const MapLayer& layer, Envelope* env) const;

  char m_szMapName[MAX_MAP_NAME];
  Envelope m_MapEnvelope;
  std::vector<MapLayer> layers_;
  int active_ = -1;
  mutable int m_nIteratorIndex = 0;
};

using Map = SmtMap;

}  // namespace gis

#endif  // _GIS_MAP_H
