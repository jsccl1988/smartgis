// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_FRAME_CAPTION_BUTTON_H_
#define UI_VIEWS_KERNEL_FRAME_CAPTION_BUTTON_H_

#include "ui/ui_export.h"

#include <functional>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

enum class CaptionButtonKind {
  kMinimize,
  kMaximize,
  kRestore,
  kClose,
};

// Title-bar control drawn with Theme caption colors.
class UI_EXPORT CaptionButton : public View {
 public:
  explicit CaptionButton(CaptionButtonKind kind);
  void set_kind(CaptionButtonKind kind);
  CaptionButtonKind kind() const { return kind_; }
  void set_click(std::function<void()> fn);

  bool on_mouse_event(const MouseEvent& e) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void activate();
  void rebuild_preferred();

  CaptionButtonKind kind_;
  std::function<void()> click_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_FRAME_CAPTION_BUTTON_H_
