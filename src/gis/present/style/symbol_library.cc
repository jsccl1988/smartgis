// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/present/style/symbol_library.h"

#include <rapidjson/document.h>

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

const rapidjson::Value* member(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) {
    return nullptr;
  }
  auto it = obj.FindMember(key);
  return it == obj.MemberEnd() ? nullptr : &it->value;
}

}  // namespace

void SymbolLibrary::clear() { entries_.clear(); }

void SymbolLibrary::set_root(const std::string& root_dir) { root_ = root_dir; }

bool SymbolLibrary::load_manifest(const char* json, size_t len) {
  if (!json || len == 0) {
    return false;
  }
  rapidjson::Document root;
  root.Parse(json, static_cast<rapidjson::SizeType>(len));
  if (root.HasParseError() || !root.IsObject()) {
    return false;
  }
  const rapidjson::Value* symbols = member(root, "symbols");
  if (!symbols || !symbols->IsArray()) {
    return false;
  }
  for (const auto& item : symbols->GetArray()) {
    if (!item.IsObject()) {
      return false;
    }
    const rapidjson::Value* id = member(item, "id");
    const rapidjson::Value* path = member(item, "path");
    if (!id || !id->IsString() || !path || !path->IsString()) {
      return false;
    }
    SymbolEntry entry;
    entry.id = std::string(id->GetString(), id->GetStringLength());
    entry.path = join_path(
        root_, std::string(path->GetString(), path->GetStringLength()));
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
