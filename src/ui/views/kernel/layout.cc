// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout.h"

#include "ui/views/kernel/view.h"

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

void BoxLayout::set_inside_border(int inset_px) {
  set_inside_border(inset_px, inset_px, inset_px, inset_px);
}

void BoxLayout::set_inside_border(int left, int top, int right, int bottom) {
  inset_left_ = left < 0 ? 0 : left;
  inset_top_ = top < 0 ? 0 : top;
  inset_right_ = right < 0 ? 0 : right;
  inset_bottom_ = bottom < 0 ? 0 : bottom;
}

void BoxLayout::set_between_child_spacing(int spacing_px) {
  between_child_ = spacing_px < 0 ? 0 : spacing_px;
}

void BoxLayout::layout(View* host) {
  if (!host) {
    return;
  }
  const Rect& host_bounds = host->bounds();
  const int inner_x = host_bounds.x + inset_left_;
  const int inner_y = host_bounds.y + inset_top_;
  const int inner_w =
      host_bounds.width - inset_left_ - inset_right_;
  const int inner_h =
      host_bounds.height - inset_top_ - inset_bottom_;
  const int host_main =
      (orientation_ == Orientation::kHorizontal) ? inner_w : inner_h;
  const int host_cross =
      (orientation_ == Orientation::kHorizontal) ? inner_h : inner_w;

  int visible_n = 0;
  int used = 0;
  int flex_sum = 0;
  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child || !child->is_visible()) {
      continue;
    }
    ++visible_n;
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
  if (visible_n > 1) {
    used += between_child_ * (visible_n - 1);
  }

  int leftover = host_main - used;
  if (leftover < 0) {
    leftover = 0;
  }

  int cursor = 0;
  int laid = 0;
  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child || !child->is_visible()) {
      continue;
    }
    if (laid > 0) {
      cursor += between_child_;
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
      r = {inner_x + cursor, inner_y, main, host_cross > 0 ? host_cross : 0};
    } else {
      r = {inner_x, inner_y + cursor, host_cross > 0 ? host_cross : 0, main};
    }
    child->set_bounds(r);
    cursor += main;
    ++laid;
  }
}

}  // namespace views
}  // namespace ui
