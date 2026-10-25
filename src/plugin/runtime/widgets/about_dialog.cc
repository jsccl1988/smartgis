// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/about_dialog.h"

#include <memory>

#include "plugin/runtime/widgets/owned_dialog.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/label.h"

namespace plugin {

AboutDialog::AboutDialog(std::string text) {
  // Label/Button allocated in plugin_host; AboutDialog itself may be new'd by
  // a product plugin — children need matching external destroyers.
  ui::views::view_adopt_child(
      this, make_plugin_view<ui::views::Label>(std::move(text)));
  ui::views::view_adopt_child(this, make_plugin_view<ui::views::Button>("OK"));
  set_preferred_size({320, 160});
}

}  // namespace plugin
