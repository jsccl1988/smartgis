// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/paint/register_default_painters.h"

#include <memory>
#include <string_view>

#include "ui/views/kernel/paint/painter.h"
#include "ui/views/kernel/paint/painter_registry.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {
namespace {

// Builtin default: forward to the View's paint_self (subclass draw).
class RoleForwardPainter final : public Painter {
 public:
  void paint(View* view, ui::gfx::Canvas* canvas) override {
    if (view) {
      view->paint_contents_for_painter(canvas);
    }
  }
};

void install_role(std::string_view role) {
  PainterRegistry::get().register_painter(
      role, std::make_unique<RoleForwardPainter>());
}

}  // namespace

void register_default_painters() {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  install_role("button");
  install_role("checkbox");
  install_role("radio_button");
  install_role("label");
  install_role("textfield");
  install_role("combobox");
  install_role("slider");
  install_role("menu_bar");
  install_role("scroll_view");
  install_role("tab_strip");
  install_role("table_view");
  install_role("tree_view");
  install_role("splitter");
  install_role("frame");
  install_role("caption_button");
}

}  // namespace views
}  // namespace ui
