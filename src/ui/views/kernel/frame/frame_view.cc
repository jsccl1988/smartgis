// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/frame/frame_view.h"

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/frame/caption_button.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

float frame_scale(View* v) {
  return (v && v->widget()) ? v->widget()->device_scale_factor() : 1.f;
}

}  // namespace

FrameView::FrameView() {
  set_layout_manager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  rebuild_caption();
}

FrameView::~FrameView() = default;

void FrameView::set_title(std::string title) {
  title_ = std::move(title);
  schedule_paint();
}

void FrameView::set_can_maximize(bool can) {
  if (can_maximize_ == can) {
    return;
  }
  can_maximize_ = can;
  // Only rebuild when there is no client yet (call before set_client).
  if (!client_) {
    rebuild_caption();
  }
}

void FrameView::set_client(std::unique_ptr<View> client) {
  rebuild_caption();
  client_ = client.get();
  if (auto* box = static_cast<BoxLayout*>(layout_manager())) {
    box->set_flex_for_view(client_, 1);
  }
  add_child(std::move(client));
}

void FrameView::rebuild_caption() {
  const float scale = frame_scale(this);
  const int caption_h = dip_to_px(32, scale);

  remove_all_children();
  caption_ = nullptr;
  client_ = nullptr;
  min_btn_ = nullptr;
  max_btn_ = nullptr;
  close_btn_ = nullptr;

  auto caption = std::make_unique<View>();
  caption->set_preferred_size({0, caption_h});
  auto row = std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);

  auto spacer = std::make_unique<View>();
  View* spacer_ptr = spacer.get();

  auto min_btn = std::make_unique<CaptionButton>(CaptionButtonKind::kMinimize);
  auto close_btn = std::make_unique<CaptionButton>(CaptionButtonKind::kClose);
  min_btn_ = min_btn.get();
  close_btn_ = close_btn.get();

  row->set_flex_for_view(spacer_ptr, 1);
  caption->set_layout_manager(std::move(row));
  caption->add_child(std::move(spacer));
  caption->add_child(std::move(min_btn));
  if (can_maximize_) {
    auto max_btn =
        std::make_unique<CaptionButton>(CaptionButtonKind::kMaximize);
    max_btn_ = max_btn.get();
    caption->add_child(std::move(max_btn));
  }
  caption->add_child(std::move(close_btn));
  caption_ = caption.get();
  add_child(std::move(caption));
  wire_buttons();
}

void FrameView::wire_buttons() {
  if (min_btn_) {
    min_btn_->set_click([this] {
      if (Widget* w = widget()) {
        ShowWindow(w->hwnd(), SW_MINIMIZE);
      }
    });
  }
  if (max_btn_) {
    max_btn_->set_click([this] {
      if (Widget* w = widget()) {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        GetWindowPlacement(w->hwnd(), &wp);
        const bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        ShowWindow(w->hwnd(), maximized ? SW_RESTORE : SW_MAXIMIZE);
        sync_maximize_button(!maximized);
      }
    });
  }
  if (close_btn_) {
    close_btn_->set_click([this] {
      if (Widget* w = widget()) {
        w->request_close();
      }
    });
  }
}

int FrameView::caption_height_px() const {
  if (!caption_) {
    return dip_to_px(32, frame_scale(const_cast<FrameView*>(this)));
  }
  return caption_->bounds().height > 0 ? caption_->bounds().height
                                       : caption_->preferred_size().height;
}

bool FrameView::point_in_caption_controls(int x, int y) const {
  auto hit = [&](CaptionButton* btn) {
    if (!btn || !btn->is_visible() || !caption_) {
      return false;
    }
    const Rect& b = btn->bounds();
    const int abs_x = caption_->bounds().x + b.x;
    const int abs_y = caption_->bounds().y + b.y;
    return x >= abs_x && x < abs_x + b.width && y >= abs_y &&
           y < abs_y + b.height;
  };
  return hit(min_btn_) || hit(max_btn_) || hit(close_btn_);
}

void FrameView::sync_maximize_button(bool maximized) {
  if (!max_btn_) {
    return;
  }
  max_btn_->set_kind(maximized ? CaptionButtonKind::kRestore
                               : CaptionButtonKind::kMaximize);
}

void FrameView::on_device_scale_factor_changed(float old_scale,
                                              float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  if (caption_) {
    caption_->set_preferred_size({0, dip_to_px(32, new_scale)});
  }
}

void FrameView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.shell_bg);
  if (caption_) {
    const Rect& c = caption_->bounds();
    canvas->fill_rect(c.x, c.y, c.width, c.height, t.caption_bg);
    if (!title_.empty()) {
      const std::wstring wide = utf8_to_wide(title_);
      const float scale = frame_scale(this);
      canvas->draw_text(c.x + dip_to_px(12, scale),
                        c.y + dip_to_px(8, scale), wide.c_str(),
                        t.text_bright);
    }
  }
}


std::string_view FrameView::paint_role() const {
  return "frame";
}
}  // namespace views
}  // namespace ui
