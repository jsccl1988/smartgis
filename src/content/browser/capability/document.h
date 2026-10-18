// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_
#define CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_

#include <concepts>
#include <string>

#include "content/browser/capability/op.h"

namespace content {

// Document lane. Mirrors GisDocument (layers, features, style, extent) plus
// GisScene store verbs that the public document type does not expose.
struct DocumentCapability {
  // Sample maps are opened by IL via resolve_data + open_document.
  detail::Op<bool(const std::string& path_utf8)> open_document;
  detail::Op<bool()> doc_clear;
  detail::Op<bool()> fit_extent;
  // |frame|: shell china_product | unit_square | document_extent, or a
  // PluginHost contribute_export_frame id.
  detail::Op<bool(const std::string& leaf, const std::string& frame)> export_bmp;
  detail::Op<bool(const std::string& path_utf8)> apply_style_file;

  // GisDocument core, opened onto the host (content/public stays unchanged).
  // |geometry|: "point" | "linestring" | "polygon".
  detail::Op<bool(const std::string& name, const std::string& geometry)> create_layer;
  detail::Op<bool(const std::string& id)> remove_layer;
  detail::Op<bool(const std::string& id, bool visible)> set_layer_visible;
  detail::Op<int()> layer_count;
  detail::Op<int()> feature_count;
  detail::Op<bool(const std::string& token,
                  const std::string& field,
                  const std::string& value)>
      update_feature_field;

  // GisScene internals (not on GisDocument).
  detail::Op<bool(const std::string& id)> select_layer;
  detail::Op<bool(const std::string& path_utf8)> write_document;
  detail::Op<bool()> clear_selection;

  // Façade reads the shell already exposes to the console bridge. Empty until
  // that bridge (or a later bind) is installed on this host. Not a public API.
  detail::Op<std::string()> layers_json;
  detail::Op<std::string()> extent_json;
  detail::Op<bool()> has_style_document;
  detail::Op<void()> clear_style;
  detail::Op<std::string()> style_summary_json;
};

template <typename T>
concept document_capability = requires(T& t, const std::string& s, bool visible) {
  { t.open_document(s) } -> std::convertible_to<bool>;
  { t.doc_clear() } -> std::convertible_to<bool>;
  { t.fit_extent() } -> std::convertible_to<bool>;
  { t.export_bmp(s, s) } -> std::convertible_to<bool>;
  { t.apply_style_file(s) } -> std::convertible_to<bool>;
  { t.create_layer(s, s) } -> std::convertible_to<bool>;
  { t.remove_layer(s) } -> std::convertible_to<bool>;
  { t.set_layer_visible(s, visible) } -> std::convertible_to<bool>;
  { t.layer_count() } -> std::convertible_to<int>;
  { t.feature_count() } -> std::convertible_to<int>;
  { t.update_feature_field(s, s, s) } -> std::convertible_to<bool>;
  { t.select_layer(s) } -> std::convertible_to<bool>;
  { t.write_document(s) } -> std::convertible_to<bool>;
  { t.clear_selection() } -> std::convertible_to<bool>;
  { t.layers_json() } -> std::convertible_to<std::string>;
  { t.extent_json() } -> std::convertible_to<std::string>;
  { t.has_style_document() } -> std::convertible_to<bool>;
  { t.clear_style() };
  { t.style_summary_json() } -> std::convertible_to<std::string>;
};

static_assert(document_capability<DocumentCapability>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_DOCUMENT_H_
