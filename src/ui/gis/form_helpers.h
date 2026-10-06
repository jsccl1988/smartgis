// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GIS_FORM_HELPERS_H_
#define UI_GIS_FORM_HELPERS_H_

#include <cctype>
#include <string>
#include <string_view>

#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"

namespace ui {
namespace views {
namespace detail {

// Trim ASCII whitespace from both ends (dialog field validation).
inline std::string trim_ascii(std::string_view in) {
  size_t begin = 0;
  while (begin < in.size() &&
         std::isspace(static_cast<unsigned char>(in[begin]))) {
    ++begin;
  }
  size_t end = in.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(in[end - 1]))) {
    --end;
  }
  return std::string(in.substr(begin, end - begin));
}

inline bool is_blank(std::string_view in) {
  return trim_ascii(in).empty();
}

// Apply industry-style validation horizon: muted hint vs danger status.
inline void set_status_label(Label* label, bool ok, const std::string& message) {
  if (!label) {
    return;
  }
  label->set_text(message);
  if (ok) {
    label->set_color(Theme::current().text_muted);
  } else {
    label->set_color(Theme::current().danger);
  }
}

inline void set_field_invalid(Textfield* field, bool invalid) {
  if (field) {
    field->set_invalid(invalid);
  }
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_GIS_FORM_HELPERS_H_
