// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/owned_dialog.h"

#include <utility>

namespace plugin {

ui::views::Dialog::Result show_owned_dialog(
    const wchar_t* title,
    int width,
    int height,
    std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return {};
  }
  if (width <= 0) {
    width = 480;
  }
  if (height <= 0) {
    height = 320;
  }
  return ui::views::Dialog::run_modal(nullptr, title, width, height,
                                      std::move(body));
}

}  // namespace plugin
