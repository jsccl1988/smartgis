// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/layer/provider_tile_layer.h"

#include <cstdio>
#include <cstring>

namespace gis {
namespace tile {

ProviderTileLayer::ProviderTileLayer(std::shared_ptr<TileProvider> provider)
    : provider_(std::move(provider)) {
  name_[0] = '\0';
  srs_[0] = '\0';
  SetLayerName("tile");
  SetSRS("EPSG:3857");
}

ProviderTileLayer::~ProviderTileLayer() { clear_tiles(); }

void ProviderTileLayer::SetLayerName(const char* szName) {
  std::snprintf(name_, sizeof(name_), "%s", szName ? szName : "");
}

void ProviderTileLayer::SetSRS(const char* szSrs) {
  std::snprintf(srs_, sizeof(srs_), "%s", szSrs ? szSrs : "");
}

void ProviderTileLayer::SetLayerRect(const Envelope& lyr_rect) {
  envelope_ = lyr_rect;
}

void ProviderTileLayer::clear_tiles() { images_.clear(); }

void ProviderTileLayer::adopt_images(std::vector<TileImage> images) {
  images_ = std::move(images);
  CalEnvelope();
  open_ = true;
}

bool ProviderTileLayer::refresh_visible(const Viewport& viewport,
                                        int timeout_sec) {
  if (!provider_ || !provider_->is_open()) {
    return false;
  }
  adopt_images(provider_->fetch_visible(viewport, timeout_sec));
  return !images_.empty();
}

void ProviderTileLayer::set_images(std::vector<TileImage> images) {
  adopt_images(std::move(images));
}

bool ProviderTileLayer::Create() {
  open_ = true;
  return true;
}

bool ProviderTileLayer::Open(const char* szLayerArchiveName) {
  if (!provider_) {
    return false;
  }
  if (szLayerArchiveName && szLayerArchiveName[0]) {
    if (!provider_->open_xyz(szLayerArchiveName)) {
      return false;
    }
  }
  open_ = provider_->is_open();
  return open_;
}

bool ProviderTileLayer::Close() {
  clear_tiles();
  open_ = false;
  return true;
}

bool ProviderTileLayer::Fetch() { return open_; }

void ProviderTileLayer::CalEnvelope() {
  envelope_ = Envelope();
  for (const TileImage& img : images_) {
    envelope_.merge(img.world_rect);
  }
}

int ProviderTileLayer::GetTileCount() const {
  return static_cast<int>(images_.size());
}

const TileImage* ProviderTileLayer::GetTile(int index) const {
  if (index < 0 || index >= static_cast<int>(images_.size())) {
    return nullptr;
  }
  return &images_[static_cast<size_t>(index)];
}

}  // namespace tile
}  // namespace gis
