// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WIDGETS_PRESENT_SURFACE_PICKER_H_
#define PLUGIN_WIDGETS_PRESENT_SURFACE_PICKER_H_

#include <functional>
#include <memory>

#include "plugin/runtime/host/plugin_host_export.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/primitives/input/combobox.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Combobox: Main window vs Map/World preview. Writes PluginHost sticky
// present_surface (0=main, 1=preview). Face still picks map vs world preview.
class PLUGIN_HOST_EXPORT PresentSurfacePicker : public ui::views::View {
 public:
  explicit PresentSurfacePicker(content::PluginHost* host);
  int surface() const;
  void set_on_surface_change(std::function<void(int surface)> fn);

 private:
  content::PluginHost* host_ = nullptr;
  ui::views::Combobox* combo_ = nullptr;
  std::function<void(int)> on_change_;
};

// Vertical stack: PresentSurfacePicker above |body| (FillLayout-friendly).
PLUGIN_HOST_EXPORT std::unique_ptr<ui::views::View> wrap_with_present_surface(
    content::PluginHost* host,
    std::unique_ptr<ui::views::View> body);

}  // namespace plugin

#endif  // PLUGIN_WIDGETS_PRESENT_SURFACE_PICKER_H_
