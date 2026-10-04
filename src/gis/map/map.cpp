// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/map/map.h"

#include <cstring>
#include <string>
#include <utility>

#include "gis/geo/ops/geometry_traits.h"
#include "ogrsf_frmts.h"
#include "gis/feature/feature.h"

namespace gis {

Map::Map() : active_(-1), m_nIteratorIndex(0) {
  strcpy_s(m_szMapName, k_map_name_max, "DefMap");
  m_MapEnvelope.MinX = 0;
  m_MapEnvelope.MinY = 0;
  m_MapEnvelope.MaxX = 800;
  m_MapEnvelope.MaxY = 600;
}

Map::~Map() { DeleteAll(); }

int Map::index_of_name(const char* szName) const {
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

const char* Map::GetLayerName(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return "";
  }
  return layers_[index].name();
}

LayerType Map::GetLayerType(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return LYR_VECTOR;
  }
  return layers_[index].layer_type();
}

bool Map::IsLayerVisible(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return false;
  }
  return layers_[index].visible();
}

void Map::SetLayerVisible(int index, bool visible) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return;
  }
  layers_[index].set_visible(visible);
}

void Map::envelope_of(const MapLayer& layer, Envelope* env) const {
  layer.get_envelope(env);
}

bool Map::AddLayer(MapLayer layer) {
  if ((!layer.ogr() && !layer.raster() && !layer.tile()) ||
      index_of_name(layer.name()) >= 0) {
    return false;
  }
  if (layers_.empty()) {
    m_MapEnvelope.MaxX = m_MapEnvelope.MinX = m_MapEnvelope.MaxY =
        m_MapEnvelope.MinY = k_envelope_unset;
  }
  Envelope lyr;
  layer.get_envelope(&lyr);
  layers_.push_back(std::move(layer));
  active_ = static_cast<int>(layers_.size()) - 1;
  m_MapEnvelope.merge(lyr);
  return true;
}

bool Map::AddLayer(OGRLayer* layer) {
  return AddLayer(MapLayer::from_ogr(layer));
}

bool Map::DeleteLayer(const char* szName) {
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

bool Map::DeleteLayer(OGRLayer* layer) {
  return layer && DeleteLayer(layer->GetName());
}

bool Map::MoveTo(int fromIndex, int toIndex) {
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

bool Map::MoveToBottom(int index) {
  return MoveTo(index, static_cast<int>(layers_.size()) - 1);
}

bool Map::MoveToTop(int index) { return MoveTo(index, 0); }

void Map::SetActiveLayer(const char* szName) {
  active_ = index_of_name(szName);
}

void Map::SetActiveOgrLayer(OGRLayer* layer) {
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

MapLayer* Map::GetActiveMapLayer() { return GetMapLayer(active_); }

const MapLayer* Map::GetActiveMapLayer() const {
  return GetMapLayer(active_);
}

OGRLayer* Map::GetActiveOgrLayer() { return GetOgrLayer(active_); }

const OGRLayer* Map::GetActiveOgrLayer() const {
  return GetOgrLayer(active_);
}

MapLayer* Map::GetMapLayer(const char* szName) {
  return GetMapLayer(index_of_name(szName));
}

const MapLayer* Map::GetMapLayer(const char* szName) const {
  return GetMapLayer(index_of_name(szName));
}

MapLayer* Map::GetMapLayer(int index) {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

const MapLayer* Map::GetMapLayer(int index) const {
  if (index < 0 || index >= static_cast<int>(layers_.size())) {
    return nullptr;
  }
  return &layers_[index];
}

OGRLayer* Map::GetOgrLayer(const char* szName) {
  return GetOgrLayer(index_of_name(szName));
}

const OGRLayer* Map::GetOgrLayer(const char* szName) const {
  return GetOgrLayer(index_of_name(szName));
}

OGRLayer* Map::GetOgrLayer(int index) {
  MapLayer* layer = GetMapLayer(index);
  return layer ? layer->ogr() : nullptr;
}

const OGRLayer* Map::GetOgrLayer(int index) const {
  const MapLayer* layer = GetMapLayer(index);
  return layer ? layer->ogr() : nullptr;
}

void Map::MoveFirst() const { m_nIteratorIndex = 0; }

void Map::MoveNext() const {
  if (m_nIteratorIndex < static_cast<int>(layers_.size())) {
    ++m_nIteratorIndex;
  }
}

void Map::MoveLast() const {
  m_nIteratorIndex = static_cast<int>(layers_.size()) - 1;
}

void Map::Delete() {
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

void Map::DeleteAll() {
  layers_.clear();
  active_ = -1;
}

bool Map::IsEnd() const {
  return m_nIteratorIndex == static_cast<int>(layers_.size());
}

void Map::CalEnvelope() {
  Envelope lyr;
  m_MapEnvelope = Envelope();
  for (MapLayer& layer : layers_) {
    layer.cal_envelope();
    envelope_of(layer, &lyr);
    m_MapEnvelope.merge(lyr);
  }
}

bool Map::AppendFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->CreateFeature(feature) == OGRERR_NONE;
}

bool Map::AppendFeature(Feature* feature, bool /*clone*/) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  OGRFeature* src = feature->ogr();
  if (src && src->GetDefnRef() == lyr->GetLayerDefn()) {
    return lyr->CreateFeature(src) == OGRERR_NONE;
  }
  OGRFeature* dst = OGRFeature::CreateFeature(lyr->GetLayerDefn());
  if (!dst) {
    return false;
  }
  if (src) {
    dst->SetFrom(src);
  } else if (feature->geometry()) {
    dst->SetGeometry(feature->geometry());
  }
  dst->SetFID(feature->id());
  const OGRErr err = lyr->CreateFeature(dst);
  OGRFeature::DestroyFeature(dst);
  return err == OGRERR_NONE;
}

bool Map::DeleteFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->DeleteFeature(feature->GetFID()) == OGRERR_NONE;
}

bool Map::UpdateFeature(OGRFeature* feature) {
  OGRLayer* lyr = GetActiveOgrLayer();
  if (!lyr || !feature) {
    return false;
  }
  return lyr->SetFeature(feature) == OGRERR_NONE;
}

namespace {

int feature_type_from_geom(const OGRGeometry* geom) {
  if (!geom) {
    return static_cast<int>(wkbUnknown);
  }
  return static_cast<int>(wkbFlatten(geom->getGeometryType()));
}

void apply_query_filters(OGRLayer* lyr, const GeomQueryDesc* gquery,
                         const AttrQueryDesc* pquery, const Envelope* env,
                         bool have_env) {
  if (have_env && env) {
    lyr->SetSpatialFilterRect(env->MinX, env->MinY, env->MaxX, env->MaxY);
  } else if (gquery && gquery->pQueryGeom) {
    lyr->SetSpatialFilter(gquery->pQueryGeom);
  }
  if (pquery && pquery->szFldName && pquery->szFldName[0] &&
      pquery->szFldQueryContent && pquery->szFldQueryContent[0]) {
    std::string attr = pquery->szFldName[0];
    attr += pquery->szFldQueryContent[0];
    if (lyr->SetAttributeFilter(attr.c_str()) != OGRERR_NONE) {
      lyr->SetAttributeFilter(nullptr);
    }
  }
}

}  // namespace

bool Map::QueryFeature(const GeomQueryDesc* gquery,
                          const AttrQueryDesc* pquery, OGRLayer* result,
                          int& n_geom_type) {
  if (!result) {
    return false;
  }
  n_geom_type = static_cast<int>(wkbUnknown);
  Envelope env;
  bool have_env = false;
  if (gquery && gquery->pQueryGeom) {
    geo::fill_envelope(*gquery->pQueryGeom, &env);
    double margin = gquery->fSmargin;
    if (margin < 0) {
      margin = 0;
    }
    // A degenerate point envelope misses nearby vertices (city dots).
    if (env.MinX == env.MaxX || env.MinY == env.MaxY) {
      if (margin <= 0) {
        margin = 1e-6;
      }
    }
    if (margin > 0) {
      env.MinX -= margin;
      env.MinY -= margin;
      env.MaxX += margin;
      env.MaxY += margin;
    }
    have_env = true;
  }

  bool queried = false;
  for (int i = 0; i < GetLayerCount(); ++i) {
    if (!IsLayerVisible(i)) {
      continue;
    }
    OGRLayer* lyr = GetOgrLayer(i);
    if (!lyr) {
      continue;
    }
    queried = true;
    apply_query_filters(lyr, gquery, pquery, &env, have_env);
    lyr->ResetReading();
    while (OGRFeature* feat = lyr->GetNextFeature()) {
      append_cloned_feature(result, feat);
      if (n_geom_type == static_cast<int>(wkbUnknown)) {
        n_geom_type = feature_type_from_geom(feat->GetGeometryRef());
      }
      OGRFeature::DestroyFeature(feat);
    }
    lyr->SetSpatialFilter(nullptr);
    lyr->SetAttributeFilter(nullptr);
  }
  return queried;
}

}  // namespace gis
