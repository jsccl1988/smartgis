// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_DIALOG_FIELDS_H_
#define PLUGIN_WIDGETS_DIALOG_FIELDS_H_

#include <string>

#include "plugin/runtime/host/plugin_host_export.h"

namespace ui {
namespace views {
class Textfield;
}  // namespace views
}  // namespace ui

namespace plugin {

// Shared field helpers for product markup dialogs (flood / traffic / …).

// Parses a Textfield-style string; empty or non-numeric → |fallback|.
PLUGIN_HOST_EXPORT double parse_field_double(const std::string& text,
                                             double fallback);

// `--plugin-sample-dir` when set, else `../data/plugin`.
PLUGIN_HOST_EXPORT std::string plugin_sample_dir();

// Sets |field| text when empty (no-op if |field| is null).
PLUGIN_HOST_EXPORT void set_textfield_if_empty(ui::views::Textfield* field,
                                               const std::string& value);

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_DIALOG_FIELDS_H_
