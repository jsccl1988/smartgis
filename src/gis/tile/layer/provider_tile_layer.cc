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
  set_name("tile");
  set_srs("EPSG:3857");
}

ProviderTileLayer::~ProviderTileLayer() { clear_tiles(); }

void ProviderTileLayer::set_name(const char* name) {
  std::snprintf(name_, sizeof(name_), "%s", name ? name : "");
}

void ProviderTileLayer::set_srs(const char* srs) {
  std::snprintf(srs_, sizeof(srs_), "%s", srs ? srs : "");
}

void ProviderTileLayer::set_rect(const Envelope& lyr_rect) {
  envelope_ = lyr_rect;
}

void ProviderTileLayer::clear_tiles() { images_.clear(); }

void ProviderTileLayer::adopt_images(std::vector<TileImage> images) {
  images_ = std::move(images);
  cal_envelope();
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

bool ProviderTileLayer::create() {
  open_ = true;
  return true;
}

bool ProviderTileLayer::open(const char* archive_name) {
  if (!provider_) {
    return false;
  }
  if (archive_name && archive_name[0]) {
    if (!provider_->open_xyz(archive_name)) {
      return false;
    }
  }
  open_ = provider_->is_open();
  return open_;
}

bool ProviderTileLayer::close() {
  clear_tiles();
  open_ = false;
  return true;
}

bool ProviderTileLayer::fetch() { return open_; }

void ProviderTileLayer::cal_envelope() {
  envelope_ = Envelope();
  for (const TileImage& img : images_) {
    envelope_.merge(img.world_rect);
  }
}

int ProviderTileLayer::tile_count() const {
  return static_cast<int>(images_.size());
}

const TileImage* ProviderTileLayer::tile_at(int index) const {
  if (index < 0 || index >= static_cast<int>(images_.size())) {
    return nullptr;
  }
  return &images_[static_cast<size_t>(index)];
}

}  // namespace tile
}  // namespace gis
