// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/style/symbol_library.h"

#include "gis/style/json_mini.h"

namespace gis {
namespace style {
namespace {

std::string join_path(const std::string& root, const std::string& rel) {
  if (root.empty() || rel.empty()) {
    return rel;
  }
  if (rel.size() >= 2 && rel[1] == ':') {
    return rel;  // Windows drive
  }
  if (rel[0] == '/' || rel[0] == '\\') {
    return rel;
  }
  char sep = '/';
  if (root.find('\\') != std::string::npos) {
    sep = '\\';
  }
  if (root.back() == '/' || root.back() == '\\') {
    return root + rel;
  }
  return root + sep + rel;
}

}  // namespace

void SymbolLibrary::clear() { entries_.clear(); }

void SymbolLibrary::set_root(const std::string& root_dir) { root_ = root_dir; }

bool SymbolLibrary::load_manifest(const char* json, size_t len) {
  detail::JsonValue root;
  if (!detail::parse_json(json, len, &root) || !root.is_object()) {
    return false;
  }
  const detail::JsonValue* symbols = root.get("symbols");
  if (!symbols || !symbols->is_array()) {
    return false;
  }
  for (const auto& item : symbols->a) {
    if (!item.is_object()) {
      return false;
    }
    const detail::JsonValue* id = item.get("id");
    const detail::JsonValue* path = item.get("path");
    if (!id || !id->is_string() || !path || !path->is_string()) {
      return false;
    }
    SymbolEntry entry;
    entry.id = id->s;
    entry.path = join_path(root_, path->s);
    upsert(std::move(entry));
  }
  return true;
}

bool SymbolLibrary::load_manifest(const std::string& json) {
  return load_manifest(json.data(), json.size());
}

void SymbolLibrary::upsert(SymbolEntry entry) {
  std::string id = entry.id;
  entries_[id] = std::move(entry);
}

bool SymbolLibrary::find(const std::string& id, SymbolEntry* out) const {
  auto it = entries_.find(id);
  if (it == entries_.end()) {
    return false;
  }
  if (out) {
    *out = it->second;
  }
  return true;
}

}  // namespace style
}  // namespace gis
