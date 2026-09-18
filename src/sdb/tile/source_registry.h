// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_TILE_SOURCE_REGISTRY_H_
#define SDB_TILE_SOURCE_REGISTRY_H_

#include "sdb/gis_export.h"
#include "sdb/tile/style_source.h"
#include "sdb/tile/tile_provider.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sdb {
namespace tile {

// Process-local map: Style source id → TileProvider (raster XYZ only).
// Does not decode MVT; binding a vector source returns kVectorUnsupported.
class GIS_EXPORT SourceRegistry {
 public:
  // Parse |source_json| and, if raster+XYZ, open a TileProvider under |id|.
  StyleSourceStatus bind_from_json(const std::string& id,
                                   const std::string& source_json);

  // Bind an already-parsed raster desc (fails for vector / non-bindable).
  StyleSourceStatus bind_raster(const StyleSourceDesc& desc);

  // Direct insert (caller owns open state). Empty id rejected.
  bool set(const std::string& id, std::shared_ptr<TileProvider> provider);

  std::shared_ptr<TileProvider> get(const std::string& id) const;
  bool contains(const std::string& id) const;
  bool remove(const std::string& id);
  void clear();
  size_t size() const { return providers_.size(); }
  std::vector<std::string> ids() const;

 private:
  std::map<std::string, std::shared_ptr<TileProvider>> providers_;
};

}  // namespace tile
}  // namespace sdb

#endif  // SDB_TILE_SOURCE_REGISTRY_H_
