// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/runtime/plugin_preview_host.h"

#include <utility>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/ui_delegate.h"
#include "content/public/view_host.h"
#include "plugin/runtime/widgets/map_preview.h"
#include "plugin/runtime/widgets/world_preview.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/viewport/draw_host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {

PluginPreviewHost::PluginPreviewHost() = default;

PluginPreviewHost::~PluginPreviewHost() {
  close();
}

bool PluginPreviewHost::is_open() const {
  return widget_ && widget_->hwnd() && IsWindow(widget_->hwnd());
}

void PluginPreviewHost::close() {
  if (map_preview_) {
    if (ui::views::DrawHost* dh = map_preview_->draw_host()) {
      dh->detach();
    }
  }
  if (world_preview_) {
    if (ui::views::DrawHost* dh = world_preview_->draw_host()) {
      dh->detach();
    }
  }
  map_preview_ = nullptr;
  world_preview_ = nullptr;
  face_ = -1;
  if (widget_) {
    widget_->request_close();
    widget_.reset();
  }
}

bool PluginPreviewHost::ensure_widget(Browser* browser, int face) {
  if (!browser) {
    return false;
  }
  // Do not call Browser::ui() from harness/runtime TUs — parallel ninja can
  // skew Browser layout so the inline accessor reads freefill (bug #10).
  const HWND owner = browser->hwnd();
  if (!owner || !IsWindow(owner)) {
    return false;
  }
  const int want = face != 0 ? 1 : 0;
  if (is_open() && face_ == want) {
    return true;
  }
  close();

  ui::views::Widget::InitParams params;
  params.title = want ? L"World preview" : L"Map preview";
  params.width = 640;
  params.height = 480;
  params.owner = owner;
  params.frame_kind = ui::views::Widget::FrameKind::kSystem;
  auto widget = std::make_unique<ui::views::Widget>();
  if (!widget->init(params)) {
    return false;
  }
  widget->set_will_close([this]() {
    map_preview_ = nullptr;
    world_preview_ = nullptr;
    face_ = -1;
  });

  if (want) {
    auto body = std::make_unique<plugin::WorldPreviewView>();
    world_preview_ = body.get();
    widget->set_contents_view(std::move(body));
  } else {
    auto body = std::make_unique<plugin::MapPreviewView>();
    map_preview_ = body.get();
    widget->set_contents_view(std::move(body));
  }
  face_ = want;
  widget_ = std::move(widget);
  widget_->show();
  widget_->layout_contents();
  return true;
}

void PluginPreviewHost::wire_draw_host(Browser* browser) {
  if (!browser) {
    return;
  }
  ui::views::DrawHost* dh = nullptr;
  content::ViewHost* vh = nullptr;
  if (face_ == 1 && world_preview_) {
    dh = world_preview_->draw_host();
    vh = browser->scene_host();
  } else if (face_ == 0 && map_preview_) {
    dh = map_preview_->draw_host();
    vh = browser->edit_host();
  }
  if (!dh) {
    return;
  }
  dh->set_view_host(vh);
  dh->set_map_contents(browser->map_session());
  if (face_ == 1) {
    dh->set_role(ui::views::DrawHost::Role::kScene3d);
  } else {
    dh->set_role(ui::views::DrawHost::Role::kMapEdit);
  }
  if (dh->attach_mode() == ui::views::DrawHost::AttachMode::kNone) {
    (void)dh->attach();
  } else {
    dh->invalidate_native();
  }
}

bool PluginPreviewHost::present(Browser* browser, int face,
                                std::string_view path) {
  if (!ensure_widget(browser, face)) {
    return false;
  }
  wire_draw_host(browser);
  if (face_ == 1 && world_preview_) {
    (void)world_preview_->open_document(path);
  } else if (map_preview_) {
    (void)map_preview_->open_document(path);
  }
  if (widget_) {
    widget_->show();
    SetForegroundWindow(widget_->hwnd());
  }
  return true;
}

bool PluginPreviewHost::export_bmp(const std::string& path) const {
  if (face_ == 1 && world_preview_) {
    return world_preview_->export_bmp(path);
  }
  if (map_preview_) {
    return map_preview_->export_bmp(path);
  }
  return false;
}

}  // namespace app
