// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_LAYOUT_SPLITTER_GRIP_H_
#define UI_VIEWS_KERNEL_LAYOUT_SPLITTER_GRIP_H_

#include "ui/ui_export.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class Splitter;

// Draggable seam between Splitter panes. Lives as the topmost child so hit
// testing wins over adjacent panes (including HWND-backed map hosts).
class UI_EXPORT SplitterGrip : public View {
 public:
  explicit SplitterGrip(Splitter* host);

  bool on_mouse_event(const MouseEvent& event) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  Splitter* host_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_LAYOUT_SPLITTER_GRIP_H_
