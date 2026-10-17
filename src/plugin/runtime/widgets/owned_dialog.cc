// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/owned_dialog.h"

#include <utility>
#include <vector>

#include "ui/views/kernel/widget/widget_window.h"

namespace plugin {
namespace {

struct DialogPresenterSlot {
  void* context = nullptr;
  DialogPresentFn fn = nullptr;
};

std::vector<DialogPresenterSlot>& dialog_presenters() {
  static std::vector<DialogPresenterSlot> slots;
  return slots;
}

}  // namespace

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
  // Owner ties z-order to the shell and lets run_modal disable it.
  const HWND owner = ui::views::primary_unowned_widget_hwnd();
  return ui::views::Dialog::run_modal(owner, title, width, height,
                                      std::move(body));
}

ui::views::Dialog::Result show_view_dialog(
    const wchar_t* title,
    std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return {};
  }
  const int width = body->preferred_size().width;
  const int height = body->preferred_size().height;
  return show_owned_dialog(title, width, height, std::move(body));
}

void set_dialog_presenter(void* context, DialogPresentFn fn) {
  std::vector<DialogPresenterSlot>& slots = dialog_presenters();
  std::erase_if(slots, [context](const DialogPresenterSlot& slot) {
    return slot.context == context;
  });
  if (fn) {
    slots.push_back(DialogPresenterSlot{context, fn});
  }
}

ui::views::Dialog::Result present_dialog(
    const wchar_t* title,
    std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return {};
  }
  const std::vector<DialogPresenterSlot>& slots = dialog_presenters();
  if (!slots.empty() && slots.back().fn) {
    slots.back().fn(slots.back().context, title, &body);
    if (!body) {
      return {};
    }
  }
  return show_view_dialog(title, std::move(body));
}

}  // namespace plugin
