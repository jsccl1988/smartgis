// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_DOCUMENT_H_
#define IL_RUNTIME_IR_DOCUMENT_H_

#include <string>
#include <string_view>

#include "app/views/il.runtime/ir/fail.h"

namespace app {
namespace ir {

// GisDocument plus MapScene store verbs. Only host.document.

inline bool open_map(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.document.open_map &&
         host.document.open_map(std::string(path));
}

inline bool doc_clear(content::CapabilityHost& host) {
  return host.document.doc_clear && host.document.doc_clear();
}

inline bool fit_extent(content::CapabilityHost& host) {
  return host.document.fit_extent && host.document.fit_extent();
}

inline bool export_bmp(content::CapabilityHost& host,
                       std::string_view leaf,
                       std::string_view frame) {
  return host.document.export_bmp &&
         host.document.export_bmp(std::string(leaf), std::string(frame));
}

inline bool apply_style_file(content::CapabilityHost& host,
                             std::string_view path) {
  return !path.empty() && host.document.apply_style_file &&
         host.document.apply_style_file(std::string(path));
}

inline bool create_layer(content::CapabilityHost& host,
                         std::string_view name,
                         std::string_view geometry) {
  return !name.empty() && host.document.create_layer &&
         host.document.create_layer(std::string(name), std::string(geometry));
}

inline bool remove_layer(content::CapabilityHost& host, std::string_view id) {
  return !id.empty() && host.document.remove_layer &&
         host.document.remove_layer(std::string(id));
}

inline bool set_layer_visible(content::CapabilityHost& host,
                              std::string_view id,
                              bool visible) {
  return !id.empty() && host.document.set_layer_visible &&
         host.document.set_layer_visible(std::string(id), visible);
}

inline bool select_layer(content::CapabilityHost& host, std::string_view id) {
  return !id.empty() && host.document.select_layer &&
         host.document.select_layer(std::string(id));
}

inline int layer_count(content::CapabilityHost& host) {
  return host.document.layer_count ? host.document.layer_count() : -1;
}

inline int feature_count(content::CapabilityHost& host) {
  return host.document.feature_count ? host.document.feature_count() : -1;
}

inline bool update_feature_field(content::CapabilityHost& host,
                                 std::string_view token,
                                 std::string_view field,
                                 std::string_view value) {
  return !token.empty() && !field.empty() &&
         host.document.update_feature_field &&
         host.document.update_feature_field(std::string(token),
                                            std::string(field),
                                            std::string(value));
}

inline bool write_map(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.document.write_map &&
         host.document.write_map(std::string(path));
}

inline bool clear_selection(content::CapabilityHost& host) {
  return host.document.clear_selection && host.document.clear_selection();
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_DOCUMENT_H_
