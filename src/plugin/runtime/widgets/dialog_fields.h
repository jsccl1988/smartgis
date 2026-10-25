// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_DIALOG_FIELDS_H_
#define PLUGIN_WIDGETS_DIALOG_FIELDS_H_

#include <cstring>
#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
class MarkupRoot;
class View;
}  // namespace views
}  // namespace ui

namespace plugin {

// Shared field helpers for product markup dialogs (flood / traffic / …).

// Parses a Textfield-style string; empty or non-numeric → |fallback|.
// Implemented in plugin_host; |text| must be a C string (not a foreign
// std::string object from another DLL).
PLUGIN_HOST_EXPORT double parse_field_double_c(const char* text,
                                               double fallback);

inline double parse_field_double(std::string_view text, double fallback) {
  if (text.empty()) {
    return fallback;
  }
  char buf[256];
  const size_t n =
      text.size() < sizeof(buf) - 1 ? text.size() : sizeof(buf) - 1;
  std::memcpy(buf, text.data(), n);
  buf[n] = '\0';
  return parse_field_double_c(buf, fallback);
}

// Copy Textfield bytes into a caller-owned std::string (inline → plugin TU).
// Do not call Textfield::text() from plugin DLLs under Debug STL.
inline std::string textfield_utf8(const ui::views::Textfield* field) {
  std::string out;
  if (!field) {
    return out;
  }
  const size_t n = field->utf8_size();
  out.resize(n);
  if (n != 0) {
    (void)field->copy_utf8(out.data(), n + 1);
  }
  return out;
}

inline double parse_textfield_double(const ui::views::Textfield* field,
                                     double fallback) {
  return parse_field_double(textfield_utf8(field), fallback);
}

inline bool textfield_utf8_empty(const ui::views::Textfield* field) {
  return !field || field->utf8_empty();
}

// `--plugin-sample-dir` when set; else absolute `<exe>/../data/plugin` (or
// `<exe>/data/plugin`) so OGR open does not depend on process CWD.
PLUGIN_HOST_EXPORT std::string plugin_sample_dir();

// Sets |field| text when empty. |utf8| is read in the caller TU (.c_str()).
inline void set_textfield_if_empty(ui::views::Textfield* field,
                                   const char* utf8) {
  if (!field || !field->utf8_empty() || !utf8 || !utf8[0]) {
    return;
  }
  field->set_text_utf8(utf8, std::strlen(utf8));
}

inline void set_textfield_if_empty(ui::views::Textfield* field,
                                   const std::string& value) {
  set_textfield_if_empty(field, value.c_str());
}

// Cross-DLL: product plugin TUs must not stack-allocate MarkupRoot or
// std::move(root) into View::add_child (Debug STL is not plugin↔ui_views ABI).
// On success returns a MarkupRoot allocated inside ui_views; use ids, then
// take_dialog_markup_root (releases the MarkupRoot).
PLUGIN_HOST_EXPORT ui::views::MarkupRoot* load_dialog_markup(
    std::string_view path_or_name);

// Releases |root->root| ownership inside ui_views and frees the MarkupRoot.
PLUGIN_HOST_EXPORT ui::views::View* take_dialog_markup_root(
    ui::views::MarkupRoot* root);

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_DIALOG_FIELDS_H_
