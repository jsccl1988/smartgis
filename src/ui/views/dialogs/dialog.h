// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_DIALOGS_DIALOG_H_
#define UI_VIEWS_DIALOGS_DIALOG_H_

#include <memory>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

class Widget;

// Modal Widget owned by |owner|. Contents may call close() to dismiss.
// |w|/|h| are client DIPs; Dialog routes through dialog_host placement
// (DPI scale, AdjustWindowRectEx, center on owner, work-area clamp).
class Dialog {
 public:
  struct Result {
    bool accepted = false;
  };

  static Result run_modal(HWND owner, const wchar_t* title, int w, int h,
                          std::unique_ptr<View> contents);

  // Dismisses the current modal dialog started by run_modal on this thread.
  static void close(bool accepted);

 private:
  Dialog() = default;

  bool accepted_ = false;
  Widget* widget_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_DIALOGS_DIALOG_H_
