// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/ws/ws.h"

namespace sdb {

SmtWSDataSource::SmtWSDataSource() = default;

SmtWSDataSource::~SmtWSDataSource() = default;

bool SmtWSDataSource::Create() { return true; }

bool SmtWSDataSource::Open() {
  open_ = true;
  return true;
}

bool SmtWSDataSource::Close() {
  open_ = false;
  return true;
}

SmtTileLayer* SmtWSDataSource::CreateTileLayer(const char* szName,
                                               fRect& lyrRect,
                                               long /*lImageCode*/) {
  auto* layer = new SmtWSTileLayer();
  layer->SetLayerName(szName);
  layer->SetLayerRect(lyrRect);
  if (!layer->Create()) {
    SMT_SAFE_DELETE(layer);
    return nullptr;
  }
  SmtLayerInfo info;
  sprintf(info.szArchiveName, "%s%s", info_.szUrl, szName);
  strcpy(info.szName, szName);
  info.unFeatureType = SmtLayer_Tile;
  layers_.push_back(info);
  return layer;
}

SmtTileLayer* SmtWSDataSource::OpenTileLayer(const char* szName) {
  auto* layer = new SmtWSTileLayer();
  if (layer->Open(szName)) {
    layer->SetLayerName(szName);
    layer->Fetch();
    return layer;
  }
  delete layer;
  return nullptr;
}

bool SmtWSDataSource::DeleteTileLayer(const char* /*szName*/) { return true; }

}  // namespace sdb
