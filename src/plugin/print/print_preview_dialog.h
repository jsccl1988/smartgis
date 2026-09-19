// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
#define PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_

#include "ui/views/kernel/view.h"

namespace plugin {

// Views-only print preview shell (MapPreviewView + Save). Not a leftover CDlg.
class PrintPreviewDialog : public ui::views::View {
 public:
  PrintPreviewDialog();
};

}  // namespace plugin

#endif  // PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
