// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
#define PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_

#include "ui/views/kernel/view/view.h"

namespace plugin {

class MapPreviewView;

// Views-only print preview shell (MapPreviewView + Save). Not a leftover CDlg.
class PrintPreviewDialog : public ui::views::View {
 public:
  PrintPreviewDialog();

 private:
  MapPreviewView* preview_ = nullptr;
};

}  // namespace plugin

#endif  // PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
