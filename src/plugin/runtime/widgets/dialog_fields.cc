// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/dialog_fields.h"

#include <cstdlib>

#include "base/process/switches.h"
#include "ui/views/primitives/text/textfield.h"

namespace plugin {

double parse_field_double(const std::string& text, double fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  return end != text.c_str() ? v : fallback;
}

std::string plugin_sample_dir() {
  const char* sample_dir = base::switch_cstr("plugin-sample-dir");
  if (sample_dir && *sample_dir) {
    return std::string(sample_dir);
  }
  return "../data/plugin";
}

void set_textfield_if_empty(ui::views::Textfield* field,
                            const std::string& value) {
  if (field && field->text().empty()) {
    field->set_text(value);
  }
}

}  // namespace plugin
