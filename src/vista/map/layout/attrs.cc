// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/attrs.h"

#include <string>

#include "gis/style/style_types.h"

namespace vista {
namespace detail {

const gis::style::AttrMap& attrs_at(const LayerBatch& batch, size_t index) {
  static const gis::style::AttrMap kEmpty;
  if (index < batch.attrs.size()) {
    return batch.attrs[index];
  }
  return kEmpty;
}

const char* attr_cstr(const gis::style::AttrMap& attrs, const char* key) {
  const auto it = attrs.find(key);
  if (it == attrs.end() || it->second.empty()) {
    return nullptr;
  }
  return it->second.c_str();
}

const char* class_cstr(const gis::style::AttrMap& attrs) {
  if (const char* cls = attr_cstr(attrs, "class")) {
    return cls;
  }
  return attr_cstr(attrs, "cls");
}

std::string expand_tokens(const std::string& field,
                          const gis::style::AttrMap& attrs) {
  std::string out;
  out.reserve(field.size());
  for (size_t i = 0; i < field.size(); ++i) {
    if (field[i] != '{') {
      out.push_back(field[i]);
      continue;
    }
    const size_t end = field.find('}', i + 1);
    if (end == std::string::npos) {
      out.push_back(field[i]);
      continue;
    }
    const std::string key = field.substr(i + 1, end - i - 1);
    const auto it = attrs.find(key);
    if (it != attrs.end()) {
      out += it->second;
    }
    i = end;
  }
  return out;
}

std::string label_text(const gis::style::ResolvedPaint& paint,
                       const gis::style::AttrMap& attrs) {
  if (!paint.text_field.empty()) {
    return expand_tokens(paint.text_field, attrs);
  }
  if (const char* anno = attr_cstr(attrs, "anno")) {
    return anno;
  }
  if (const char* name = attr_cstr(attrs, "name")) {
    return name;
  }
  if (const char* text = attr_cstr(attrs, "text")) {
    return text;
  }
  return {};
}

}  // namespace detail
}  // namespace vista
