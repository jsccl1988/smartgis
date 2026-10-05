// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Theme, painter registry, canvas, and kernel focus unit tests.

#include <memory>
#include <string>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"
#include "ui/gfx/color/color.h"
#include "ui/views/kernel/frame/caption_button.h"
#include "ui/views/kernel/paint/painter.h"
#include "ui/views/kernel/paint/painter_registry.h"
#include "ui/views/kernel/paint/register_default_painters.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/primitives/text/textfield.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;

void test_utf8_and_theme() {
  expect(utf8_to_wide("ok") == L"ok", "utf8_to_wide");
  expect(wide_to_utf8(L"ok") == "ok", "wide_to_utf8");
  ThemeService::get().ensure_builtin_packs();
  expect(ThemeService::get().set_theme("dark"), "set dark");
  expect(Theme::current().accent == ui::gfx::color_rgb(0, 122, 204),
         "dark theme accent");
  expect(ThemeService::get().set_theme("light"), "set light");
  expect(Theme::current().shell_bg == ui::gfx::color_rgb(245, 245, 245),
         "light shell_bg");
  expect(!ThemeService::get().set_theme("missing"), "unknown theme fails");
  expect(ThemeService::get().set_theme("dark"), "restore dark");
}

void test_painter_registry_and_delegate() {
  register_default_painters();
  expect(PainterRegistry::get().find("button") != nullptr,
         "builtin button painter");

  class CountingPainter final : public Painter {
   public:
    int paints = 0;
    void paint(View* view, ui::gfx::Canvas* canvas) override {
      ++paints;
      (void)view;
      (void)canvas;
    }
  };
  class OrderDelegate final : public PaintDelegate {
   public:
    std::string order;
    void paint_before(View*, ui::gfx::Canvas*) override { order += 'B'; }
    void paint_after(View*, ui::gfx::Canvas*) override { order += 'A'; }
  };
  class RoleView final : public View {
   public:
    int self_paints = 0;
    std::string_view paint_role() const override { return "unittest_role"; }

   protected:
    void paint_self(ui::gfx::Canvas*) override { ++self_paints; }
  };

  auto painter = std::make_unique<CountingPainter>();
  CountingPainter* raw = painter.get();
  PainterRegistry::get().register_painter("unittest_role", std::move(painter));

  RoleView view;
  view.set_bounds({0, 0, 20, 20});
  OrderDelegate del;
  view.set_paint_delegate(&del);

  ui::gfx::DisplayList list;
  view.append_commands_to(&list);
  expect(raw->paints == 1, "registry painter used");
  expect(view.self_paints == 0, "paint_self skipped when painter set");
  expect(del.order == "BA", "delegate before then after");

  auto plug = std::make_unique<CountingPainter>();
  CountingPainter* plug_raw = plug.get();
  PainterRegistry::get().register_painter_for_plugin(
      "unittest_plug", "unittest_role", std::move(plug));
  view.invalidate_commands();
  del.order.clear();
  ui::gfx::DisplayList list2;
  view.append_commands_to(&list2);
  expect(plug_raw->paints == 1, "plugin painter used");
  expect(raw->paints == 1, "builtin not called while overridden");

  PainterRegistry::get().withdraw_plugin("unittest_plug");
  expect(PainterRegistry::get().find("unittest_role") == raw,
         "withdraw restores prior painter");
}

void test_shell_canvas_preference() {
  ui::gfx::apply_shell_canvas_preference("gdi");
  expect(ui::gfx::resolved_shell_canvas_backend() ==
             ui::gfx::ShellCanvasBackend::kGdi,
         "prefer gdi");
  const auto skia = ui::gfx::apply_shell_canvas_preference("skia");
  if (ui::gfx::is_skia_backend_available()) {
    expect(skia == ui::gfx::ShellCanvasBackend::kSkia, "skia when linked");
  } else {
    expect(skia == ui::gfx::ShellCanvasBackend::kGdi, "skia falls back to gdi");
  }
  ui::gfx::apply_shell_canvas_preference("gdi");
}

void test_skia_canvas_api() {
  ui::gfx::apply_shell_canvas_preference("gdi");
  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  const int W = 64;
  const int H = 32;
  HBITMAP bmp = CreateCompatibleBitmap(screen, W, H);
  HGDIOBJ old = SelectObject(mem, bmp);

  ui::gfx::Canvas c(mem, W, H);
  c.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 0));
  c.stroke_rect(2, 2, 20, 10, ui::gfx::color_rgb(255, 0, 0), 1);
  c.draw_line(0, 0, 10, 10, ui::gfx::color_rgb(0, 255, 0), 1);
  c.save();
  c.clip_rect(8, 8, 16, 16);
  c.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 255));
  c.restore();
  const auto sz = c.measure_text(L"Ab");
  expect(sz.width > 0 && sz.height > 0, "measure_text Ab");
  expect(c.measure_text(L"").width == 0, "measure_text empty");
  expect(c.measure_text(nullptr).width == 0, "measure_text null");

  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (ui::gfx::is_skia_backend_available()) {
    ui::gfx::apply_shell_canvas_preference("skia");
    HDC screen2 = GetDC(nullptr);
    HDC mem2 = CreateCompatibleDC(screen2);
    HBITMAP bmp2 = CreateCompatibleBitmap(screen2, W, H);
    HGDIOBJ old2 = SelectObject(mem2, bmp2);
    ui::gfx::Canvas c2(mem2, W, H);
    c2.fill_rect(0, 0, W, H, ui::gfx::color_rgb(0, 0, 0));
    const auto sz2 = c2.measure_text(L"Ab");
    expect(sz2.width > 0 && sz2.height > 0, "skia measure_text Ab");
    SelectObject(mem2, old2);
    DeleteObject(bmp2);
    DeleteDC(mem2);
    ReleaseDC(nullptr, screen2);
    ui::gfx::apply_shell_canvas_preference("gdi");
  }
}

void test_kernel_visible_enabled_focus_hover() {
  View hidden;
  hidden.set_bounds({0, 0, 40, 40});
  hidden.set_visible(false);
  expect(!hidden.is_visible(), "view hidden");
  expect(hidden.get_view_at(10, 10) == nullptr, "hidden skips hit-test");

  View disabled;
  disabled.set_enabled(false);
  expect(!disabled.is_enabled(), "view disabled");

  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* button = new Button("Go");
  button->set_bounds({0, 0, 80, 28});
  root->add_child(std::unique_ptr<View>(button));
  widget.set_contents_view(std::move(root));

  expect(button->is_visible(), "button visible");
  expect(button->is_enabled(), "button enabled");
  expect(button->request_focus(), "button focus");
  expect(button->is_focused(), "button is focused");
  expect(widget.focused_view() == button, "widget focused_view");

  widget.send_mouse(mouse_move(10, 10));
  expect(button->is_hovered(), "button hovered");
  expect(widget.hovered_view() == button, "widget hovered_view");

  button->set_enabled(false);
  expect(!button->is_enabled(), "button disabled");
  expect(!button->is_focused(), "disabled clears focus");
  expect(!button->request_focus(), "disabled cannot focus");

  button->set_enabled(true);
  button->set_visible(false);
  expect(!button->is_visible(), "button hidden");
  expect(!button->request_focus(), "hidden cannot focus");
}

void test_tab_focus_traversal() {
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 400, 300});
  auto* a = new Button("A");
  auto* mid = new Button("hidden");
  auto* b = new Textfield();
  a->set_bounds({0, 0, 80, 28});
  mid->set_bounds({0, 30, 80, 28});
  mid->set_visible(false);
  b->set_bounds({0, 60, 160, 24});
  root->add_child(std::unique_ptr<View>(a));
  root->add_child(std::unique_ptr<View>(mid));
  root->add_child(std::unique_ptr<View>(b));
  widget.set_contents_view(std::move(root));

  expect(a->request_focus(), "tab start A");
  expect(widget.send_key(key_down(VK_TAB)), "tab key");
  expect(b->is_focused(), "tab skips hidden");
  expect(!mid->is_focused(), "hidden not focused");
  expect(widget.advance_focus(true), "shift-tab reverse");
  expect(a->is_focused(), "reverse back to A");
}

void test_caption_button_release_outside_cancels() {
  int clicks = 0;
  Widget widget;
  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 200, 80});
  auto btn = std::make_unique<CaptionButton>(CaptionButtonKind::kClose);
  CaptionButton* close = btn.get();
  close->set_bounds({0, 0, 46, 32});
  close->set_click([&clicks]() { ++clicks; });
  root->add_child(std::move(btn));
  widget.set_contents_view(std::move(root));

  expect(widget.send_mouse(mouse_down(10, 10)), "caption down");
  expect(widget.send_mouse(mouse_up(120, 60)), "caption up outside");
  expect(clicks == 0, "release outside does not activate");

  expect(widget.send_mouse(mouse_down(10, 10)), "caption down 2");
  expect(widget.send_mouse(mouse_up(10, 10)), "caption up inside");
  expect(clicks == 1, "release inside activates");
}

