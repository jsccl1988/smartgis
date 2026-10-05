// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
#define PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Print chrome: Save exports a composed page. The map face is the main Views
// Map tab (PluginHost::present_dataset), not a nested MapPreviewView.
class PrintPreviewDialog : public ui::views::View {
 public:
  explicit PrintPreviewDialog(content::PluginHost* host);
};

}  // namespace plugin

#endif  // PLUGIN_PRINT_PRINT_PREVIEW_DIALOG_H_
