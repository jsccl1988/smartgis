// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/widgets/about_dialog.h"

#include <memory>

#include "ui/views/primitives/button.h"
#include "ui/views/primitives/label.h"

namespace plugin {

AboutDialog::AboutDialog(std::string text) {
  auto label = std::make_unique<ui::views::Label>(std::move(text));
  auto ok = std::make_unique<ui::views::Button>("OK");
  add_child(std::move(label));
  add_child(std::move(ok));
  set_preferred_size({320, 160});
}

}  // namespace plugin
