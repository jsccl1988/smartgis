// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/harness/views_test_base.h"

namespace ui {
namespace views {

void make_test_widget(TestWidgetRoot* out, int width, int height) {
  if (!out) {
    return;
  }
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, width, height});
  out->root = root.get();
  out->widget.set_contents_view(std::move(root));
}

View* find_child_at(View* root, int x, int y) {
  if (!root) {
    return nullptr;
  }
  return root->get_view_at(x, y);
}

}  // namespace views
}  // namespace ui
