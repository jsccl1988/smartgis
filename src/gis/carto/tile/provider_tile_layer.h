// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_PROVIDER_TILE_LAYER_H_
#define SDB_TILE_PROVIDER_TILE_LAYER_H_

#include <memory>
#include <vector>

#include "gis/envelope.h"
#include "gis/gis_export.h"
#include "gis/map/layer_kind.h"
#include "gis/carto/tile/tile_provider.h"

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

  bool Create();
  bool Open(const char* szLayerArchiveName);
  bool Close();
  bool Fetch();
  bool IsOpen() const { return open_; }
  void CalEnvelope();
  void get_envelope(Envelope& env) const { env = envelope_; }

  void SetLayerName(const char* szName);
  const char* GetLayerName() const { return name_; }
  void SetSRS(const char* szSrs);
  const char* GetSRS() const { return srs_; }
  void SetLayerRect(const Envelope& lyr_rect);
  LayerType GetLayerType() const { return LYR_TITLE; }

  int GetTileCount() const;
  const TileImage* GetTile(int index) const;
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

#endif  // SDB_TILE_PROVIDER_TILE_LAYER_H_
