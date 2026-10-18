// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_UTILS_VIEW_TREE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_UTILS_VIEW_TREE_H_

#include <sstream>
#include <string_view>

#include "ui/views/kernel/view/view.h"

namespace content {
namespace detail {

// Depth-first lookup by paint_role. Shared by Debug Console UI and IL probes.
inline const ui::views::View* find_view_by_paint_role(
    const ui::views::View* root,
    std::string_view name) {
  if (!root) {
    return nullptr;
  }
  if (root->paint_role() == name) {
    return root;
  }
  for (size_t i = 0; i < root->child_count(); ++i) {
    if (const ui::views::View* hit =
            find_view_by_paint_role(root->child_at(i), name)) {
      return hit;
    }
  }
  return nullptr;
}

// Pretty-print a View subtree (role + bounds) for console / IL dump.
inline void append_view_tree_lines(const ui::views::View* view,
                                   int depth,
                                   std::ostringstream& oss) {
  if (!view) {
    return;
  }
  for (int i = 0; i < depth; ++i) {
    oss << ' ';
  }
  const std::string_view role = view->paint_role();
  const ui::views::Rect b = view->bounds();
  oss << (role.empty() ? "view" : role) << " [" << b.x << ',' << b.y << ' '
      << b.width << 'x' << b.height << "]\n";
  for (size_t i = 0; i < view->child_count(); ++i) {
    append_view_tree_lines(view->child_at(i), depth + 1, oss);
  }
}

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_UTILS_VIEW_TREE_H_
