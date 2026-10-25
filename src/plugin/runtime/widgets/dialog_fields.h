// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_DIALOG_FIELDS_H_
#define PLUGIN_WIDGETS_DIALOG_FIELDS_H_

#include <string>
#include <string_view>

#include "plugin/runtime/host/plugin_host_export.h"

namespace ui {
namespace views {
class MarkupRoot;
class Textfield;
class View;
}  // namespace views
}  // namespace ui

namespace plugin {

// Shared field helpers for product markup dialogs (flood / traffic / …).

// Parses a Textfield-style string; empty or non-numeric → |fallback|.
PLUGIN_HOST_EXPORT double parse_field_double(const std::string& text,
                                             double fallback);

// `--plugin-sample-dir` when set; else absolute `<exe>/../data/plugin` (or
// `<exe>/data/plugin`) so OGR open does not depend on process CWD.
PLUGIN_HOST_EXPORT std::string plugin_sample_dir();

// Sets |field| text when empty (no-op if |field| is null).
PLUGIN_HOST_EXPORT void set_textfield_if_empty(ui::views::Textfield* field,
                                               const std::string& value);

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
