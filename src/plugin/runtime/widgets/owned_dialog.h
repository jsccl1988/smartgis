// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_OWNED_DIALOG_H_
#define PLUGIN_WIDGETS_OWNED_DIALOG_H_

#include <memory>
#include <utility>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/view/view.h"

namespace plugin {

// Allocates T in the calling TU and registers an external destroyer so the
// shell (ui_views) can clear the form without freeing on the wrong heap.
template <typename T, typename... Args>
T* make_plugin_view(Args&&... args) {
  T* p = new T(std::forward<Args>(args)...);
  p->set_external_destroy([](ui::views::View* v) {
    delete static_cast<T*>(v);
  });
  return p;
}

template <typename T, typename... Args>
std::unique_ptr<ui::views::View> make_plugin_view_unique(Args&&... args) {
  return std::unique_ptr<ui::views::View>(
      make_plugin_view<T>(std::forward<Args>(args)...));
}

// Shows |body| in a modal Widget that owns it until the dialog closes.
// Dialog factories must use this instead of a stack View: open_dialog
// returns as soon as the factory returns, so a stack dialog is destroyed
// before it can be seen.
PLUGIN_HOST_EXPORT ui::views::Dialog::Result show_owned_dialog(
    const wchar_t* title,
    int width,
    int height,
    std::unique_ptr<ui::views::View> body);

// Convenience: size from |body|->preferred_size() (falls back inside
// show_owned_dialog when preferred size is empty).
PLUGIN_HOST_EXPORT ui::views::Dialog::Result show_view_dialog(
    const wchar_t* title,
    std::unique_ptr<ui::views::View> body);

// Shell panel host. |fn| must take ownership by moving out of |body|.
// The newest non-null registration wins; clear with a null |fn| for |context|.
using DialogPresentFn = void (*)(void* context,
                                 const wchar_t* title,
                                 std::unique_ptr<ui::views::View>* body);

PLUGIN_HOST_EXPORT void set_dialog_presenter(void* context, DialogPresentFn fn);

// Hands |body| to the active presenter. With no presenter, falls back to
// show_view_dialog so tests and harness keep a modal.
PLUGIN_HOST_EXPORT ui::views::Dialog::Result present_dialog(
    const wchar_t* title,
    std::unique_ptr<ui::views::View> body);

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_OWNED_DIALOG_H_
