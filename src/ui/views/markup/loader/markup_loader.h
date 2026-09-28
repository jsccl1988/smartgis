// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_LOADER_MARKUP_LOADER_H_
#define UI_VIEWS_MARKUP_LOADER_MARKUP_LOADER_H_

#include "ui/ui_export.h"
#include <memory>
#include <string>
#include <string_view>

#include "ui/views/markup/factory/control_factory.h"
#include "ui/views/markup/document/markup_document.h"
#include "ui/views/markup/document/named_view_map.h"

namespace ui {
namespace views {

class View;

struct UI_EXPORT MarkupOptions {
  const ControlFactory* factory = nullptr;
};

// Owns the loaded root View and id map.
struct UI_EXPORT MarkupRoot {
  std::unique_ptr<View> root;
  NamedViewMap ids;
  std::string name;
  std::string error;

  bool ok() const { return root != nullptr; }
};

// Build a View tree from a MarkupDocument.
UI_EXPORT bool build_markup_tree(const MarkupDocument& doc,
                                       const MarkupOptions& options,
                                       MarkupRoot* out);

// Parse XML bytes and build the tree.
UI_EXPORT bool load_markup_bytes(std::string_view xml_utf8,
                                       std::string_view base_dir,
                                       const MarkupOptions& options,
                                       MarkupRoot* out);

// Load by path or resource basename (e.g. "add_basemap.ui.xml").
// Searches: absolute path, <exe>/../ui/<rel> (shared out/ui), <exe>/ui/<rel>,
// cwd ui/<rel>, source src/ui/resources/<rel>.
// Use resolve_markup_path() when the caller needs the resolved disk path
// (do not grow MarkupRoot across the DLL boundary — NRVO into a mismatched
// caller slot corrupts trailing fields).
UI_EXPORT MarkupRoot load_markup(std::string_view path_or_name,
                                       const MarkupOptions& options = {});

// Resolve a markup path or basename to an existing file path (UTF-8).
// Returns empty string when not found.
UI_EXPORT std::string resolve_markup_path(std::string_view path_or_name);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_LOADER_MARKUP_LOADER_H_
