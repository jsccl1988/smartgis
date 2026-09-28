// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_OWNED_DIALOG_H_
#define PLUGIN_WIDGETS_OWNED_DIALOG_H_

#include <memory>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/dialogs/dialog.h"

namespace plugin {

// Shows |body| in a modal Widget that owns it until the dialog closes.
// Dialog factories must use this instead of a stack View: open_dialog
// returns as soon as the factory returns, so a stack dialog is destroyed
// before it can be seen.
PLUGIN_HOST_EXPORT ui::views::Dialog::Result show_owned_dialog(
    const wchar_t* title,
    int width,
    int height,
    std::unique_ptr<ui::views::View> body);

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_OWNED_DIALOG_H_
