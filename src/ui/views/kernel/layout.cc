// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/layout.h"

#include "ui/views/view.h"

namespace ui {
namespace views {

void FillLayout::layout(View* host) {
  if (!host) {
    return;
  }
  const Rect& b = host->bounds();
  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child || !child->is_visible()) {
      continue;
    }
    child->set_bounds(b);
  }
}

BoxLayout::BoxLayout(Orientation orientation) : orientation_(orientation) {}

void BoxLayout::set_flex_for_view(const View* view, int flex) {
  flex_[view] = flex;
}

void BoxLayout::layout(View* host) {
  if (!host) {
    return;
  }
  const Rect& host_bounds = host->bounds();
  const int host_main = (orientation_ == Orientation::kHorizontal)
                            ? host_bounds.width
                            : host_bounds.height;
  const int host_cross = (orientation_ == Orientation::kHorizontal)
                             ? host_bounds.height
                             : host_bounds.width;

  int used = 0;
  int flex_sum = 0;
  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child || !child->is_visible()) {
      continue;
    }
    const int flex = flex_.count(child) ? flex_[child] : 0;
    if (flex > 0) {
      flex_sum += flex;
    } else {
      const int pref = (orientation_ == Orientation::kHorizontal)
                           ? child->preferred_size().width
                           : child->preferred_size().height;
      used += pref;
    }
  }

  int leftover = host_main - used;
  if (leftover < 0) {
    leftover = 0;
  }

  int cursor = 0;
  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child || !child->is_visible()) {
      continue;
    }
    const int flex = flex_.count(child) ? flex_[child] : 0;
    int main = 0;
    if (flex > 0 && flex_sum > 0) {
      main = leftover * flex / flex_sum;
    } else {
      main = (orientation_ == Orientation::kHorizontal)
                 ? child->preferred_size().width
                 : child->preferred_size().height;
    }
    Rect r;
    if (orientation_ == Orientation::kHorizontal) {
      r = {host_bounds.x + cursor, host_bounds.y, main, host_cross};
    } else {
      r = {host_bounds.x, host_bounds.y + cursor, host_cross, main};
    }
    child->set_bounds(r);
    cursor += main;
  }
}

}  // namespace views
}  // namespace ui
