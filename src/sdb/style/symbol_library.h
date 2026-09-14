// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_STYLE_SYMBOL_LIBRARY_H_
#define SDB_STYLE_SYMBOL_LIBRARY_H_

#include "sdb/gis_export.h"
#include "sdb/style/style_types.h"

#include <string>
#include <unordered_map>

namespace sdb {
namespace style {

// External symbol id → path (and optional bytes) registry.
class GIS_EXPORT SymbolLibrary {
 public:
  void clear();
  void set_root(const std::string& root_dir);
  const std::string& root() const { return root_; }

  // Manifest: {"symbols":[{"id":"marker","path":"marker.png"}, ...]}
  bool load_manifest(const char* json, size_t len);
  bool load_manifest(const std::string& json);

  void upsert(SymbolEntry entry);
  bool find(const std::string& id, SymbolEntry* out) const;
  size_t size() const { return entries_.size(); }

 private:
  std::string root_;
  std::unordered_map<std::string, SymbolEntry> entries_;
};

}  // namespace style
}  // namespace sdb

#endif  // SDB_STYLE_SYMBOL_LIBRARY_H_
