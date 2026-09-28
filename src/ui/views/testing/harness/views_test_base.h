// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_HARNESS_VIEWS_TEST_BASE_H_
#define UI_VIEWS_TESTING_HARNESS_VIEWS_TEST_BASE_H_

#include <memory>

#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

// Owns a headless Widget and its root View with client bounds set.
struct TestWidgetRoot {
  Widget widget;
  View* root = nullptr;
};

// Builds a Widget with an empty root View sized to |width| x |height| (DIPs).
void make_test_widget(TestWidgetRoot* out, int width, int height);

// Hit-test in |root|'s coordinate space (same as Widget client coords when
// |root| is the contents view with origin at 0,0).
View* find_child_at(View* root, int x, int y);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TESTING_HARNESS_VIEWS_TEST_BASE_H_
