// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_
#define CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_

#include <functional>
#include <string>

namespace content {

// Document lane. Mirrors GisDocument (layers, features, style, extent) plus
// MapScene store verbs that the public document type does not expose.
struct DocumentCapability {
  // Sample maps are opened by IL via resolve_data + open_map.
  std::function<bool(const std::string& path_utf8)> open_map;
  std::function<bool()> doc_clear;
  std::function<bool()> fit_extent;
  // |frame|: shell china_product | unit_square | document_extent, or a
  // PluginHost contribute_export_frame id.
  std::function<bool(const std::string& leaf, const std::string& frame)>
      export_bmp;
  std::function<bool(const std::string& path_utf8)> apply_style_file;

  // GisDocument core, opened onto the host (content/public stays unchanged).
  // |geometry|: "point" | "linestring" | "polygon".
  std::function<bool(const std::string& name, const std::string& geometry)>
      create_layer;
  std::function<bool(const std::string& id)> remove_layer;
  std::function<bool(const std::string& id, bool visible)> set_layer_visible;
  std::function<int()> layer_count;
  std::function<int()> feature_count;
  std::function<bool(const std::string& token, const std::string& field,
                     const std::string& value)>
      update_feature_field;

  // MapScene internals (not on GisDocument).
  std::function<bool(const std::string& id)> select_layer;
  std::function<bool(const std::string& path_utf8)> write_map;
  std::function<bool()> clear_selection;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_
