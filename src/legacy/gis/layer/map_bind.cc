// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/layer/map_bind.h"

#include "legacy/gis/layer/raster_wrap.h"
#include "legacy/gis/layer/tile_wrap.h"

namespace gis {
namespace {

Envelope env_from_rect(const fRect& r) {
  Envelope e;
  e.MinX = r.lb.x;
  e.MinY = r.lb.y;
  e.MaxX = r.rt.x;
  e.MaxY = r.rt.y;
  return e;
}

}  // namespace

bool leftover_add_layer(Map* map, Layer* layer, bool owns) {
  if (!map || !layer) {
    return false;
  }
  if (auto* wrap = dynamic_cast<LeftoverOgrRasterLayer*>(layer)) {
    datasource::OgrRasterLayer* inner = wrap->release_inner();
    if (owns) {
      delete wrap;
    }
    return map->AddLayer(MapLayer::from_raster(inner, true));
  }
  if (auto* wrap = dynamic_cast<LeftoverProviderTileLayer*>(layer)) {
    tile::ProviderTileLayer* inner = wrap->release_inner();
    if (owns) {
      delete wrap;
    }
    return map->AddLayer(MapLayer::from_tile(inner, true));
  }
  (void)owns;
  (void)env_from_rect;
  return false;
}

Layer* leftover_layer_at(Map* map, int index) {
  if (!map) {
    return nullptr;
  }
  MapLayer* ml = map->GetMapLayer(index);
  if (!ml) {
    return nullptr;
  }
  if (ml->raster()) {
    return new LeftoverOgrRasterLayer(ml->raster(), false);
  }
  if (ml->tile()) {
    return new LeftoverProviderTileLayer(ml->tile(), false);
  }
  return nullptr;
}

const Layer* leftover_layer_at(const Map* map, int index) {
  return leftover_layer_at(const_cast<Map*>(map), index);
}

Layer* leftover_layer_named(Map* map, const char* name) {
  if (!map) {
    return nullptr;
  }
  MapLayer* ml = map->GetMapLayer(name);
  if (!ml) {
    return nullptr;
  }
  for (int i = 0; i < map->GetLayerCount(); ++i) {
    if (map->GetMapLayer(i) == ml) {
      return leftover_layer_at(map, i);
    }
  }
  return nullptr;
}

Layer* leftover_active_layer(Map* map) {
  if (!map) {
    return nullptr;
  }
  MapLayer* ml = map->GetActiveMapLayer();
  if (!ml) {
    return nullptr;
  }
  for (int i = 0; i < map->GetLayerCount(); ++i) {
    if (map->GetMapLayer(i) == ml) {
      return leftover_layer_at(map, i);
    }
  }
  return nullptr;
}

}  // namespace gis
