// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_PRINT_PRINT_PREVIEW_DIALOG_H_
#define PLUGIN_MAP2D_PRINT_PRINT_PREVIEW_DIALOG_H_

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Print horizon: Save exports a composed page. The map face is the main Views
// Map tab, not a nested MapPreviewView. Construction must stay UI-light:
// no present_dataset / Widget / modal (those run only from open_dialog).
class PrintPreviewDialog : public ui::views::View {
 public:
  explicit PrintPreviewDialog(content::PluginHost* host);
};

// Dialog factory body. Call only from PluginHost::open_dialog, never from
// contribute_print / register_map2d.
void open_print_preview_dialog(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_PRINT_PRINT_PREVIEW_DIALOG_H_
