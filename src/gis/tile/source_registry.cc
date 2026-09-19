// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/source_registry.h"

namespace gis {
namespace tile {

StyleSourceStatus SourceRegistry::bind_from_json(
    const std::string& id, const std::string& source_json) {
  StyleSourceDesc desc;
  const StyleSourceStatus st = parse_style_source(id, source_json, &desc);
  if (st == StyleSourceStatus::kVectorUnsupported) {
    return st;
  }
  if (st != StyleSourceStatus::kOk) {
    return st;
  }
  return bind_raster(desc);
}

StyleSourceStatus SourceRegistry::bind_raster(const StyleSourceDesc& desc) {
  if (desc.type == StyleSourceType::kVector) {
    return StyleSourceStatus::kVectorUnsupported;
  }
  if (!is_raster_bindable(desc) || desc.id.empty()) {
    if (desc.type == StyleSourceType::kUnsupported) {
      return StyleSourceStatus::kUnsupportedType;
    }
    if (desc.id.empty()) {
      return StyleSourceStatus::kMissingId;
    }
    if (desc.tiles.empty()) {
      return StyleSourceStatus::kMissingTiles;
    }
    return StyleSourceStatus::kBadUrlTemplate;
  }
  auto provider = std::make_shared<TileProvider>();
  if (!open_provider_from_source(desc, provider.get())) {
    return StyleSourceStatus::kBadUrlTemplate;
  }
  providers_[desc.id] = std::move(provider);
  return StyleSourceStatus::kOk;
}

bool SourceRegistry::set(const std::string& id,
                         std::shared_ptr<TileProvider> provider) {
  if (id.empty() || !provider) {
    return false;
  }
  providers_[id] = std::move(provider);
  return true;
}

std::shared_ptr<TileProvider> SourceRegistry::get(const std::string& id) const {
  auto it = providers_.find(id);
  return it == providers_.end() ? nullptr : it->second;
}

bool SourceRegistry::contains(const std::string& id) const {
  return providers_.find(id) != providers_.end();
}

bool SourceRegistry::remove(const std::string& id) {
  return providers_.erase(id) > 0;
}

void SourceRegistry::clear() { providers_.clear(); }

std::vector<std::string> SourceRegistry::ids() const {
  std::vector<std::string> out;
  out.reserve(providers_.size());
  for (const auto& kv : providers_) {
    out.push_back(kv.first);
  }
  return out;
}

}  // namespace tile
}  // namespace gis
