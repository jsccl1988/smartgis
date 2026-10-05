// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_TILE_PROVIDER_TILE_LAYER_H_
#define GIS_TILE_PROVIDER_TILE_LAYER_H_

#include <memory>
#include <vector>

#include "gis/envelope.h"
#include "gis/gis_export.h"
#include "gis/map/layer_kind.h"
#include "gis/tile/provider/tile_provider.h"

namespace gis {
namespace tile {

// Tile cursor over TileProvider visible set. Product type; leftover TileLayer
// wrap lives in leftover/gis/layer/tile_wrap.h.
class GIS_EXPORT ProviderTileLayer {
 public:
  explicit ProviderTileLayer(std::shared_ptr<TileProvider> provider);
  ~ProviderTileLayer();

  std::shared_ptr<TileProvider> provider() const { return provider_; }

  bool refresh_visible(const Viewport& viewport, int timeout_sec = 5);
  void set_images(std::vector<TileImage> images);

  bool create();
  bool open(const char* archive_name);
  bool close();
  bool fetch();
  bool is_open() const { return open_; }
  void cal_envelope();
  void get_envelope(Envelope& env) const { env = envelope_; }

  void set_name(const char* name);
  const char* name() const { return name_; }
  void set_srs(const char* srs);
  const char* srs() const { return srs_; }
  void set_rect(const Envelope& lyr_rect);
  LayerType layer_type() const { return LayerType::kTile; }

  int tile_count() const;
  const TileImage* tile_at(int index) const;
  const std::vector<TileImage>& images() const { return images_; }

 private:
  void clear_tiles();
  void adopt_images(std::vector<TileImage> images);

  std::shared_ptr<TileProvider> provider_;
  std::vector<TileImage> images_;
  Envelope envelope_;
  char name_[256];
  char srs_[256];
  bool open_ = false;
};

}  // namespace tile
}  // namespace gis

#endif  // GIS_TILE_PROVIDER_TILE_LAYER_H_
