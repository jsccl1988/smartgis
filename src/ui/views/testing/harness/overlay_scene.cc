// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/harness/overlay_scene.h"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/color/color.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/paint/register_default_painters.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/gfx/canvas/canvas.h"

namespace ui {
namespace views {
namespace {

constexpr const char* kBuiltinRoles[] = {
    "button",   "label",      "checkbox", "textfield", "combobox",
    "slider",   "tab_strip",  "table_view", "scroll_view", "menu_bar",
};

class RoleLayerView : public View {
 public:
  explicit RoleLayerView(std::string role) : role_(std::move(role)) {}

  std::string_view paint_role() const override { return role_; }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Rect& b = bounds();
    const Theme& t = Theme::current();
    ui::gfx::Color fill = t.control_fill;
    canvas->fill_rect(b.x, b.y, b.width, b.height, fill);
  }

 private:
  std::string role_;
};

}  // namespace

OverlayScene::OverlayScene(int width, int height, int layer_count)
    : width_(width), height_(height), layer_count_(layer_count) {
  register_default_painters();
  ThemeService::get().ensure_builtin_packs();
  rebuild_layers();
}

void OverlayScene::set_layer_count(int n) {
  if (n < 1) {
    n = 1;
  }
  if (n == layer_count_) {
    return;
  }
  layer_count_ = n;
  rebuild_layers();
}

void OverlayScene::rebuild_layers() {
  auto host = std::make_unique<View>();
  host->set_bounds({0, 0, width_, height_});
  host->set_layout_manager(std::make_unique<FillLayout>());

  const size_t role_count = sizeof(kBuiltinRoles) / sizeof(kBuiltinRoles[0]);
  for (int i = 0; i < layer_count_; ++i) {
    const char* role = kBuiltinRoles[static_cast<size_t>(i) % role_count];
    auto layer = std::make_unique<RoleLayerView>(role);
    host->add_child(std::move(layer));
  }
  host->layout();
  root_ = std::move(host);
}

std::uint64_t OverlayScene::measure_commit_ns(int iters) const {
  if (!root_ || iters <= 0 || width_ <= 0 || height_ <= 0) {
    return 0;
  }

  const Rect dirty{0, 0, width_, height_};
  const ui::gfx::Color clear = Theme::current().shell_bg;
  PaintCommit frame;

  LARGE_INTEGER freq{};
  QueryPerformanceFrequency(&freq);
  LARGE_INTEGER t0{};
  QueryPerformanceCounter(&t0);
  for (int i = 0; i < iters; ++i) {
    commit_view_tree(root_.get(), dirty, width_, height_, 12, clear, &frame);
  }
  LARGE_INTEGER t1{};
  QueryPerformanceCounter(&t1);

  const std::uint64_t elapsed =
      static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart);
  const double seconds =
      static_cast<double>(elapsed) / static_cast<double>(freq.QuadPart);
  const double ns_total = seconds * 1e9;
  return static_cast<std::uint64_t>(ns_total / static_cast<double>(iters));
}

}  // namespace views
}  // namespace ui
