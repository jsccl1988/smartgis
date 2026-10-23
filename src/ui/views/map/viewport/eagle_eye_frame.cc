// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/viewport/draw_host.h"

#include "content/public/types.h"
#include "ui/views/map/eagle_eye/eagle_eye_host.h"

namespace ui {
namespace views {

EagleEyeHost* DrawHost::eagle_eye() {
  if (role_ != Role::kMapEdit) {
    return nullptr;
  }
  if (!eagle_eye_) {
    eagle_eye_ = std::make_unique<EagleEyeHost>();
  }
  return eagle_eye_.get();
}

void DrawHost::set_eagle_eye_enabled(bool enabled) {
  if (EagleEyeHost* eye = eagle_eye()) {
    eye->set_visible(enabled);
    sync_eagle_eye();
  }
}

bool DrawHost::eagle_eye_enabled() const {
  if (role_ != Role::kMapEdit || !eagle_eye_) {
    return EagleEyeHost::load_persisted_visible();
  }
  return eagle_eye_->visible();
}

void DrawHost::toggle_eagle_eye() {
  if (EagleEyeHost* eye = eagle_eye()) {
    eye->toggle_visible();
    sync_eagle_eye();
  }
}

void DrawHost::set_eagle_eye_world_extent(const content::Extent2& world) {
  if (EagleEyeHost* eye = eagle_eye()) {
    eye->set_world_extent(world);
  }
}

void DrawHost::set_eagle_eye_view_extent(const content::Extent2& view) {
  if (EagleEyeHost* eye = eagle_eye()) {
    eye->set_view_extent(view);
  }
}

void DrawHost::set_eagle_eye_commit(
    std::function<void(const content::Extent2& extent)> fn) {
  if (EagleEyeHost* eye = eagle_eye()) {
    eye->set_commit_fn(std::move(fn));
  }
}

void DrawHost::sync_eagle_eye() {
  if (role_ != Role::kMapEdit) {
    if (eagle_eye_) {
      eagle_eye_->destroy();
    }
    return;
  }
  EagleEyeHost* eye = eagle_eye();
  if (!eye) {
    return;
  }
  HWND surface = native_view();
  if (gpu_present_hwnd_ && IsWindow(gpu_present_hwnd_) &&
      IsWindowVisible(gpu_present_hwnd_)) {
    surface = gpu_present_hwnd_;
  }
  if (!eye->visible() || !surface || !IsWindow(surface) ||
      !IsWindowVisible(surface)) {
    eye->destroy();
    return;
  }
  eye->sync_to_surface(surface);
}

}  // namespace views
}  // namespace ui
