// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
#define PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_

#include "ui/views/view.h"

namespace plugin {

// Print preview shell: shared MapPreviewView plus a Save button.
class PrintPreviewDialog : public ui::views::View {
 public:
  PrintPreviewDialog();
};

}  // namespace plugin

#endif  // PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
