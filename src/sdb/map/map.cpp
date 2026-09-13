// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/map/map.h"

#include "algorithm/geo/geometry.h"
#include "sdb/feature/feature_api.h"

#include "ogrsf_frmts.h"

#include <cstring>
#include <string>
#include <utility>

namespace sdb {

SmtMap::SmtMap() : active_(-1), m_nIteratorIndex(0) {
  strcpy_s(m_szMapName, MAX_MAP_NAME, "DefMap");
  m_MapEnvelope.MinX = 0;
  m_MapEnvelope.MinY = 0;
  m_MapEnvelope.MaxX = 800;
  m_MapEnvelope.MaxY = 600;
}

SmtMap::~SmtMap() { DeleteAll(); }

int SmtMap::index_of_name(const char* szName) const {
  if (!szName) {
    return -1;
  }
  for (int i = 0; i < static_cast<int>(layers_.size()); ++i) {
    const char* name = GetLayerName(i);
    if (name && std::strcmp(name, szName) == 0) {
      return i;
    }
  }
  return -1;
}

const char* SmtMap::GetLayerName(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return "";
  }
  return layers_[index].name();
}

SmtLayerType SmtMap::GetLayerType(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return LYR_VECTOR;
  }
  return layers_[index].layer_type();
}

bool SmtMap::IsLayerVisible(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return false;
  }
  return layers_[index].visible();
}

void SmtMap::SetLayerVisible(int index, bool visible) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return;
  }
  layers_[index].set_visible(visible);
}

void SmtMap::envelope_of(const MapLayer& layer, Envelope* env) const {
  layer.get_envelope(env);
}

bool SmtMap::AddLayer(MapLayer layer) {
  if ((!layer.ogr() && !layer.leftover()) ||
      index_of_name(layer.name()) >= 0) {
    return false;
  }
  if (layers_.empty()) {
    m_MapEnvelope.MaxX = m_MapEnvelope.MinX = m_MapEnvelope.MaxY =
        m_MapEnvelope.MinY = SMT_C_INVALID_DBF_VALUE;
  }
  Envelope lyr;
  layer.get_envelope(&lyr);
  layers_.push_back(std::move(layer));
  active_ = static_cast<int>(layers_.size()) - 1;
  m_MapEnvelope.merge(lyr);
  return true;
}

bool SmtMap::AddLayer(OGRLayer* layer) {
  return AddLayer(MapLayer::from_ogr(layer));
}

bool SmtMap::AddLayer(SmtLayer* layer) {
  return AddLayer(MapLayer::from_leftover(layer, true));
}

bool SmtMap::DeleteLayer(const char* szName) {
  const int i = index_of_name(szName);
  if (i < 0) {
    return false;
  }
  layers_.erase(layers_.begin() + i);
  if (active_ == i) {
    active_ = -1;
  } else if (active_ > i) {
    --active_;
  }
  CalEnvelope();
  return true;
}

bool SmtMap::DeleteLayer(OGRLayer* layer) {
  return layer && DeleteLayer(layer->GetName());
}

bool SmtMap::DeleteLayer(const SmtLayer* layer) {
  return layer && DeleteLayer(layer->GetLayerName());
}

bool SmtMap::MoveTo(int fromIndex, int toIndex) {
  if (fromIndex < 0 || toIndex < 0 ||
      fromIndex >= static_cast<int>(layers_.size()) ||
      toIndex >= static_cast<int>(layers_.size())) {
    return false;
  }
  std::swap(layers_[fromIndex], layers_[toIndex]);
  if (active_ == fromIndex) {
    active_ = toIndex;
  } else if (active_ == toIndex) {
    active_ = fromIndex;
  }
  return true;
}

bool SmtMap::MoveToBottom(int index) {
  return MoveTo(index, static_cast<int>(layers_.size()) - 1);
}

bool SmtMap::MoveToTop(int index) { return MoveTo(index, 0); }

void SmtMap::SetActiveLayer(const char* szName) {
  active_ = index_of_name(szName);
}

void SmtMap::SetActiveOgrLayer(OGRLayer* layer) {
  if (!layer) {
    active_ = -1;
    return;
  }
  for (int i = 0; i < static_cast<int>(layers_.size()); ++i) {
    if (layers_[i].ogr() == layer) {
      active_ = i;
      return;
    }
  }
}

MapLayer* SmtMap::GetActiveMapLayer() { return GetMapLayer(active_); }

const MapLayer* SmtMap::GetActiveMapLayer() const {
  return GetMapLayer(active_);
}

OGRLayer* SmtMap::GetActiveOgrLayer() { return GetOgrLayer(active_); }

const OGRLayer* SmtMap::GetActiveOgrLayer() const {
  return GetOgrLayer(active_);
}

SmtLayer* SmtMap::GetActiveLeftoverLayer() {
  return GetLeftoverLayer(active_);
}

const SmtLayer* SmtMap::GetActiveLeftoverLayer() const {
  return GetLeftoverLayer(active_);
}

MapLayer* SmtMap::GetMapLayer(const char* szName) {
  return GetMapLayer(index_of_name(szName));
}

const MapLayer* SmtMap::GetMapLayer(const char* szName) const {
  return GetMapLayer(index_of_name(szName));
}

MapLayer* SmtMap::GetMapLayer(int index) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

const MapLayer* SmtMap::GetMapLayer(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

OGRLayer* SmtMap::GetOgrLayer(const char* szName) {
  return GetOgrLayer(index_of_name(szName));
}

const OGRLayer* SmtMap::GetOgrLayer(const char* szName) const {
  return GetOgrLayer(index_of_name(szName));
}

SmtLayer* SmtMap::GetLeftoverLayer(const char* szName) {
  return GetLeftoverLayer(index_of_name(szName));
}

const SmtLayer* SmtMap::GetLeftoverLayer(const char* szName) const {
  return GetLeftoverLayer(index_of_name(szName));
}

OGRLayer* SmtMap::GetOgrLayer(int index) {
  MapLayer* layer = GetMapLayer(index);
  return layer ? layer->ogr() : nullptr;
}

const OGRLayer* SmtMap::GetOgrLayer(int index) const {
  const MapLayer* layer = GetMapLayer(index);
  return layer ? layer->ogr() : nullptr;
}

SmtLayer* SmtMap::GetLeftoverLayer(int index) {
  MapLayer* layer = GetMapLayer(index);
  return layer ? layer->leftover() : nullptr;
}

const SmtLayer* SmtMap::GetLeftoverLayer(int index) const {
  const MapLayer* layer = GetMapLayer(index);
  return layer ? layer->leftover() : nullptr;
}

void SmtMap::MoveFirst() const { m_nIteratorIndex = 0; }

void SmtMap::MoveNext() const {
  if (m_nIteratorIndex < static_cast<int>(layers_.size())) {
    ++m_nIteratorIndex;
  }
}

void SmtMap::MoveLast() const {
  m_nIteratorIndex = static_cast<int>(layers_.size()) - 1;
}

void SmtMap::Delete() {
  if (m_nIteratorIndex < 0 ||
      m_nIteratorIndex >= static_cast<int>(layers_.size())) {
    return;
  }
  layers_.erase(layers_.begin() + m_nIteratorIndex);
  if (active_ == m_nIteratorIndex) {
    active_ = -1;
  } else if (active_ > m_nIteratorIndex) {
    --active_;
  }
  CalEnvelope();
}

void SmtMap::DeleteAll() {
  layers_.clear();
  active_ = -1;
}

bool SmtMap::IsEnd() const {
  return m_nIteratorIndex == static_cast<int>(layers_.size());
}

void SmtMap::CalEnvelope() {
  Envelope lyr;
  m_MapEnvelope = Envelope();
  for (MapLayer& layer : layers_) {
    layer.cal_envelope();
    envelope_of(layer, &lyr);
    m_MapEnvelope.merge(lyr);
  }
}

bool SmtMap::AppendFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->CreateFeature(feature) == OGRERR_NONE;
}

bool SmtMap::AppendFeature(SmtFeature* feature, bool /*clone*/) {
  return leftover_append_feature(GetActiveOgrLayer(), feature);
}

bool SmtMap::DeleteFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->DeleteFeature(feature->GetFID()) == OGRERR_NONE;
}

bool SmtMap::UpdateFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->SetFeature(feature) == OGRERR_NONE;
}

bool SmtMap::QueryFeature(const SmtGQueryDesc* gquery,
                          const SmtPQueryDesc* pquery, OGRLayer* result,
                          int& nFeaType) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !result) {
    return false;
  }
  nFeaType = SmtFtUnknown;
  if (gquery && gquery->pQueryGeom) {
    Envelope env;
    geo::copy_envelope(*gquery->pQueryGeom, &env);
    lyr->SetSpatialFilterRect(env.MinX, env.MinY, env.MaxX, env.MaxY);
  }
  if (pquery && pquery->szFldName && pquery->szFldName[0] &&
      pquery->szFldQueryContent && pquery->szFldQueryContent[0]) {
    std::string attr = pquery->szFldName[0];
    attr += pquery->szFldQueryContent[0];
    if (lyr->SetAttributeFilter(attr.c_str()) != OGRERR_NONE) {
      lyr->SetAttributeFilter(nullptr);
    }
  }
  lyr->ResetReading();
  while (OGRFeature* feat = lyr->GetNextFeature()) {
    result->CreateFeature(feat);
    OGRFeature::DestroyFeature(feat);
  }
  lyr->SetSpatialFilter(nullptr);
  lyr->SetAttributeFilter(nullptr);
  return true;
}

}  // namespace sdb
