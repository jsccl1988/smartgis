// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_PROVIDER_TILE_LAYER_H_
#define SDB_TILE_PROVIDER_TILE_LAYER_H_

#include "sdb/gis_export.h"
#include "sdb/layer/layer.h"
#include "sdb/tile/tile_provider.h"

#include <memory>
#include <vector>

namespace sdb {
namespace tile {

// SmtTileLayer adapter over TileProvider visible set. Not a mem/DLL subclass;
// owns encoded tile bytes for tessellate_tile_layer / attach_tile_layer.
class GIS_EXPORT ProviderTileLayer : public SmtTileLayer {
 public:
  explicit ProviderTileLayer(std::shared_ptr<TileProvider> provider);
  ~ProviderTileLayer() override;

  std::shared_ptr<TileProvider> provider() const { return provider_; }

  // Pull tiles for viewport into the leftover SmtTile cursor table.
  bool refresh_visible(const Viewport& viewport, int timeout_sec = 5);
  // Offline / test: install already-fetched images.
  void set_images(std::vector<TileImage> images);

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(eSmtFetchType type = FETCH_ALL) override;
  void CalEnvelope() override;

  int GetTileCount() const override;
  void MoveFirst() const override;
  void MoveNext() const override;
  void MoveLast() const override;
  void Delete() override;
  bool IsEnd() const override;
  void DeleteAll() override;

  long AppendTile(const SmtTile* pTile, bool bClone = false) override;
  long UpdateTile(const SmtTile* pTile) override;
  long DeleteTile(const SmtTile* pTile) override;
  SmtTile* GetTile() const override;
  SmtTile* GetTile(int index) const override;
  SmtTile* GetTileByID(uint unID) const override;

 private:
  void clear_tiles();
  void adopt_images(std::vector<TileImage> images);

  std::shared_ptr<TileProvider> provider_;
  std::vector<SmtTile*> tiles_;
  mutable int cursor_ = 0;
};

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_PROVIDER_TILE_LAYER_H_
