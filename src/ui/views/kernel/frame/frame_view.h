// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_FRAME_FRAME_VIEW_H_
#define UI_VIEWS_KERNEL_FRAME_FRAME_VIEW_H_

#include "ui/ui_export.h"

#include <memory>
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

class CaptionButton;

// Custom window horizon: caption strip + client content slot.
class UI_EXPORT FrameView : public View {
 public:
  FrameView();
  ~FrameView() override;

  void set_title(std::string title);
  const std::string& title() const { return title_; }

  // When false, hide maximize/restore (typical for modal dialogs).
  void set_can_maximize(bool can);
  bool can_maximize() const { return can_maximize_; }

  // Own the client content (menu + workspace). Replaces any previous client.
  void set_client(std::unique_ptr<View> client);
  View* client() const { return client_; }

  int caption_height_px() const;
  bool point_in_caption_controls(int x, int y) const;

  void sync_maximize_button(bool maximized);

  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void rebuild_caption();
  void wire_buttons();

  std::string title_;
  bool can_maximize_ = true;
  View* caption_ = nullptr;
  View* client_ = nullptr;
  CaptionButton* min_btn_ = nullptr;
  CaptionButton* max_btn_ = nullptr;
  CaptionButton* close_btn_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_FRAME_FRAME_VIEW_H_
