// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_TEXT_LABEL_H_
#define UI_VIEWS_PRIMITIVES_TEXT_LABEL_H_

#include "ui/ui_views_export.h"
#include <string>

#include "ui/gfx/color/color.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Static text node. Color defaults to the current Theme text.
class UI_VIEWS_EXPORT Label : public View {
 public:
  explicit Label(std::string text);
  void set_text(std::string text);
  const std::string& text() const;
  void set_color(ui::gfx::Color color);
  void clear_color();
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_text_cache();

  std::string text_;
  std::wstring wide_;
  Size ink_{};
  float ink_scale_ = -1.f;
  ui::gfx::Color color_ = 0;
  bool has_color_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_TEXT_LABEL_H_
