// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_MAP_H
#define _GIS_MAP_H

#include "base/style/envelope.h"
#include "sdb/feature/feature.h"
#include "sdb/gis_export.h"
#include "sdb/layer/layer.h"

#include <vector>

class OGRFeature;
class OGRLayer;

using namespace base;
using namespace base;

#define MAX_MAP_NAME MAX_NAME_LENGTH

namespace sdb {

// Map document. Vector layers are OGRLayer; raster/tile leftovers are SmtLayer.
class GIS_EXPORT SmtMap {
 public:
  SmtMap();
  virtual ~SmtMap();

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

  OGRLayer* GetActiveOgrLayer();
  const OGRLayer* GetActiveOgrLayer() const;
  SmtLayer* GetActiveLeftoverLayer();
  const SmtLayer* GetActiveLeftoverLayer() const;

  // Leftover names used by MFC / plugin TUs after the OGR cut-over.
  SmtLayer* GetActiveLayer() { return GetActiveLeftoverLayer(); }
  const SmtLayer* GetActiveLayer() const { return GetActiveLeftoverLayer(); }
  SmtLayer* GetLayer(const char* szName) { return GetLeftoverLayer(szName); }
  const SmtLayer* GetLayer(const char* szName) const {
    return GetLeftoverLayer(szName);
  }
  SmtLayer* GetLayer(int index) { return GetLeftoverLayer(index); }
  const SmtLayer* GetLayer(int index) const { return GetLeftoverLayer(index); }
  SmtLayer* GetLayer() { return GetActiveLeftoverLayer(); }
  bool AddLayer(const SmtLayer* layer) {
    return AddLayer(const_cast<SmtLayer*>(layer));
  }

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
  struct Entry {
    SmtLayerType type = LYR_VECTOR;
    OGRLayer* ogr = nullptr;
    SmtLayer* leftover = nullptr;
    bool visible = true;
    bool owns_leftover = true;
  };

  int index_of_name(const char* szName) const;
  void envelope_of(const Entry& e, Envelope* env) const;

  char m_szMapName[MAX_MAP_NAME];
  Envelope m_MapEnvelope;
  std::vector<Entry> layers_;
  int active_ = -1;
  mutable int m_nIteratorIndex = 0;
};

}  // namespace sdb

#endif  // _GIS_MAP_H
