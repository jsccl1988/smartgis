// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/tile/tile_map_layer.h"

#include "sdb/tile/provider_tile_layer.h"

namespace sdb {
namespace tile {

MapLayer make_map_layer(std::shared_ptr<TileProvider> provider) {
  auto* layer = new ProviderTileLayer(std::move(provider));
  return MapLayer::from_leftover(layer, true);
}

MapLayer make_xyz_map_layer(const std::string& url_template) {
  auto provider = std::make_shared<TileProvider>();
  if (!provider->open_xyz(url_template)) {
    return MapLayer{};
  }
  return make_map_layer(std::move(provider));
}

MapLayer make_wmts_map_layer(const std::string& url_template) {
  auto provider = std::make_shared<TileProvider>();
  if (!provider->open_wmts_template(url_template)) {
    return MapLayer{};
  }
  return make_map_layer(std::move(provider));
}

MapLayer make_wmts_map_layer_from_capabilities(const std::string& xml) {
  auto provider = std::make_shared<TileProvider>();
  if (!provider->open_wmts_capabilities(xml)) {
    return MapLayer{};
  }
  return make_map_layer(std::move(provider));
}

}  // namespace tile
}  // namespace sdb
