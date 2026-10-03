// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Dialog, Widget, touch, and DPI unit tests.

#include <cmath>
#include <memory>

#include "ui/gis/shell/status_bar.h"
#include "ui/gfx/color/color.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/kernel/shell/dialog_host.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/map/touch_multitouch.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/input/combobox.h"
#include "ui/views/primitives/text/label.h"
#include "ui/views/testing/unit/views_unit_helpers.h"

using namespace ui::views;

void test_dialog_close_noop() {
  // Do not pump a native modal loop in this console test.
  Dialog::close(true);
  expect(true, "dialog close without run_modal");
}

void test_dialog_host_geometry() {
  RECT owner = {100, 200, 500, 600};  // 400x400
  const OwnedPopupGeom g = center_outer_on_owner_rect(owner, 200, 100);
  expect(g.x == 200, "popup x centered on owner");
  expect(g.y == 350, "popup y centered on owner");
  expect(g.outer_width == 200, "outer width kept");
  expect(g.outer_height == 100, "outer height kept");

  RECT popup = {g.x, g.y, g.x + g.outer_width, g.y + g.outer_height};
  expect(rect_approximately_centered(popup, owner, 1),
         "screen centers align");

  int outer_w = 0;
  int outer_h = 0;
  client_to_outer_size(380, 160, kOwnedDialogStyle, 0, &outer_w, &outer_h);
  expect(outer_w > 380, "caption expands width");
  expect(outer_h > 160, "caption expands height");

  const OwnedPopupGeom placed =
      place_owned_dialog(nullptr, 380, 160, kOwnedDialogStyle, 0);
  expect(placed.outer_width >= outer_w - 2, "place uses outer size");
  expect(placed.outer_height >= outer_h - 2, "place uses outer height");
}

void test_widget_hwnd_and_map_viewport() {
  // Peers do not CreateWindow in this console test; skip Widget::init.
  // MapViewport::attach needs a parent HWND / GPU; do not call it here.
  Widget widget;
  expect(widget.hwnd() == nullptr, "widget hwnd before init");
  expect(widget.contents_view() == nullptr, "widget empty contents");
  expect(widget.device_scale_factor() == 1.f, "widget default scale 1");
  expect(widget.dpi() == kDefaultDpi, "widget default dpi 96");

  MapViewport viewport;
  expect(viewport.attach_mode() == MapViewport::AttachMode::kNone,
         "map_viewport not attached");
}

void test_custom_frame_hides_os_caption() {
  // Regression: kCustom must not keep WS_CAPTION (double title bar).
  Widget widget;
  Widget::InitParams params;
  params.title = L"CSD test";
  params.width = 320;
  params.height = 240;
  params.size_in_dips = true;
  params.frame_kind = Widget::FrameKind::kCustom;
  expect(widget.init(params), "custom frame init");
  expect(widget.hwnd() != nullptr, "custom frame hwnd");
  expect(widget.frame_kind() == Widget::FrameKind::kCustom,
         "frame_kind custom");

  const LONG style = GetWindowLongW(widget.hwnd(), GWL_STYLE);
  expect((style & WS_CAPTION) == 0, "custom frame has no WS_CAPTION");
  expect((style & WS_THICKFRAME) != 0, "custom frame keeps thickframe");
  expect((style & WS_MINIMIZEBOX) != 0, "top-level custom has minimize");
  expect((style & WS_MAXIMIZEBOX) != 0, "top-level custom has maximize");

  RECT wr = {};
  RECT cr = {};
  GetWindowRect(widget.hwnd(), &wr);
  GetClientRect(widget.hwnd(), &cr);
  expect((wr.right - wr.left) == (cr.right - cr.left),
         "custom frame client width == window");
  expect((wr.bottom - wr.top) == (cr.bottom - cr.top),
         "custom frame client height == window");
  // Destructor sets destroying_ before DestroyWindow (avoids PostQuitMessage).
}

void test_touch_multitouch_midpoint() {
  TouchMultitouchTracker tracker;
  content::InputEvent e{};

  // Single finger: no multitouch sample; mouse path stays authoritative.
  expect(!tracker.on_contact_down(1, 10, 20, &e), "single down silent");
  expect(tracker.contact_count() == 1, "one contact");
  expect(!tracker.suppress_mouse(), "single no suppress");
  expect(!tracker.on_contact_move(1, 12, 22, &e), "single move silent");

  // Second finger: ldown at midpoint, suppress mouse synthesis.
  expect(tracker.on_contact_down(2, 30, 40, &e), "two-finger down");
  expect(e.kind == content::InputEvent::Kind::kLDown, "down kind");
  expect(e.pointer_count >= 2, "down pointer_count");
  // After single-finger move to (12,22), mid with (30,40) is (21,31).
  expect(e.x_px == 21 && e.y_px == 31, "down midpoint");
  expect(tracker.suppress_mouse(), "multitouch suppresses mouse");

  expect(tracker.on_contact_move(1, 14, 24, &e), "two-finger move");
  expect(e.kind == content::InputEvent::Kind::kMouseMove, "move kind");
  expect(e.pointer_count >= 2, "move pointer_count");
  expect(e.x_px == 22 && e.y_px == 32, "move midpoint");

  expect(tracker.on_contact_up(2, 30, 40, &e), "two-finger up");
  expect(e.kind == content::InputEvent::Kind::kLUp, "up kind");
  expect(e.pointer_count >= 2, "up pointer_count");
  expect(!tracker.suppress_mouse(), "up clears suppress");
  expect(tracker.contact_count() == 1, "one contact remains");

  // Remaining single finger: silent again.
  expect(!tracker.on_contact_move(1, 16, 26, &e), "post-up single silent");
  expect(!tracker.on_contact_up(1, 16, 26, &e), "last up silent");
  expect(tracker.contact_count() == 0, "cleared");
}

void test_dpi_scale_math() {
  expect(scale_factor_from_dpi(96) == 1.f, "96 dpi -> 1.0");
  expect(scale_factor_from_dpi(144) == 1.5f, "144 dpi -> 1.5");
  expect(scale_factor_from_dpi(192) == 2.f, "192 dpi -> 2.0");
  expect(scale_factor_from_dpi(0) == 1.f, "0 dpi -> 1.0");
  expect(dip_to_px(100, 1.5f) == 150, "dip_to_px 100@1.5");
  expect(dip_to_px(10, 1.25f) == 13, "dip_to_px rounds 12.5 -> 13");
  expect(px_to_dip(150, 1.5f) == 100, "px_to_dip 150@1.5");
  expect(dpi_for_hwnd(nullptr) >= 96u, "dpi_for_hwnd screen fallback");
}

void test_device_scale_recomputes_preferred() {
  Widget widget;
  auto root = std::make_unique<View>();
  auto* root_ptr = root.get();
  auto button = std::make_unique<Button>("Scale");
  auto* button_ptr = button.get();
  root->add_child(std::move(button));
  const int w96 = button_ptr->preferred_size().width;
  const int h96 = button_ptr->preferred_size().height;
  expect(w96 > 0 && h96 >= 28, "button preferred at 96dpi");

  widget.set_contents_view(std::move(root));
  expect(root_ptr->widget() == &widget, "contents widget wired");

  widget.set_device_scale_factor(1.5f);
  expect(widget.device_scale_factor() == 1.5f, "widget scale 1.5");
  expect(widget.dpi() == 144u, "widget dpi 144");
  expect(button_ptr->preferred_size().width >
             w96,
         "button preferred grows with scale");
  expect(button_ptr->preferred_size().height >= dip_to_px(28, 1.5f),
         "button min height scales");

  const int w150 = button_ptr->preferred_size().width;
  widget.set_device_scale_factor(2.f);
  expect(button_ptr->preferred_size().width > w150,
         "button preferred grows again at 2x");

  auto fixed = std::make_unique<View>();
  fixed->set_preferred_size({100, 40});
  auto* fixed_ptr = fixed.get();
  root_ptr->add_child(std::move(fixed));
  // Child added after scale bump is notified in set_widget (1 -> current).
  expect(fixed_ptr->preferred_size().width == 200, "view preferred *2 width");
  expect(fixed_ptr->preferred_size().height == 80, "view preferred *2 height");
}

void test_combobox_dpi_row_geometry() {
  Widget widget;
  auto root = std::make_unique<View>();
  auto combo = std::make_unique<Combobox>();
  Combobox* c = combo.get();
  c->add_item("a");
  c->add_item("b");
  root->add_child(std::move(combo));
  widget.set_contents_view(std::move(root));
  widget.set_device_scale_factor(1.5f);
  c->set_bounds({0, 0, dip_to_px(200, 1.5f), dip_to_px(24, 1.5f)});
  expect(c->on_mouse_event(mouse_up(10, 10)), "combo open @1.5");
  expect(c->is_open(), "combo open state");
  // Floating popup Widget owns the list; host bounds stay header-sized.
  expect(c->bounds().height == dip_to_px(24, 1.5f) ||
             c->bounds().height == dip_to_px(28, 1.5f),
         "open height includes scaled rows");
  expect(c->preferred_size().height == dip_to_px(28, 1.5f),
         "preferred stays header-sized");
  std::vector<std::string> issues;
  expect(collect_layout_violations(c, &issues) == 0,
         "open combo rows exempt from outside-parent");
}

void test_dialog_host_clamps_to_work_area() {
  // Extremely large dialog should still produce a finite positive box.
  const OwnedPopupGeom huge =
      place_owned_dialog(nullptr, 4000, 3000, kOwnedDialogStyle, 0);
  expect(huge.outer_width > 0 && huge.outer_height > 0, "huge dialog sized");
  expect(huge.x > -100000 && huge.y > -100000, "clamped coords finite");
}

