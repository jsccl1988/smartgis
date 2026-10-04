// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/style/flex_style.h"

namespace ui {
namespace views {

namespace {

template <typename T>
void merge_opt(std::optional<T>* dst, const std::optional<T>& src) {
  if (src.has_value()) {
    *dst = src;
  }
}

}  // namespace

void FlexStyle::merge_from(const FlexStyle& other) {
  merge_opt(&display, other.display);
  merge_opt(&flex_direction, other.flex_direction);
  merge_opt(&justify_content, other.justify_content);
  merge_opt(&align_items, other.align_items);
  merge_opt(&flex, other.flex);
  merge_opt(&flex_grow, other.flex_grow);
  merge_opt(&flex_shrink, other.flex_shrink);
  merge_opt(&gap, other.gap);
  merge_opt(&padding, other.padding);
  merge_opt(&padding_left, other.padding_left);
  merge_opt(&padding_top, other.padding_top);
  merge_opt(&padding_right, other.padding_right);
  merge_opt(&padding_bottom, other.padding_bottom);
  merge_opt(&margin, other.margin);
  merge_opt(&margin_left, other.margin_left);
  merge_opt(&margin_top, other.margin_top);
  merge_opt(&margin_right, other.margin_right);
  merge_opt(&margin_bottom, other.margin_bottom);
  merge_opt(&width, other.width);
  merge_opt(&height, other.height);
  merge_opt(&min_width, other.min_width);
  merge_opt(&min_height, other.min_height);
  merge_opt(&max_width, other.max_width);
  merge_opt(&max_height, other.max_height);
  merge_opt(&color, other.color);
  merge_opt(&background_color, other.background_color);
  merge_opt(&font_size, other.font_size);
}

}  // namespace views
}  // namespace ui
