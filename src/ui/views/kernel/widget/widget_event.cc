// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/widget/widget_event.h"

#include <string>
#include <vector>

#include <imm.h>
#include <windowsx.h>

#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

void collect_focusable_views(View* root, std::vector<View*>* out) {
  if (!root || !out || !root->is_visible()) {
    return;
  }
  if (root->is_focusable() && root->is_enabled()) {
    out->push_back(root);
  }
  for (size_t i = 0; i < root->child_count(); ++i) {
    collect_focusable_views(root->child_at(i), out);
  }
}

MouseEvent make_mouse_event(MouseEvent::Type type,
                            WPARAM wparam,
                            LPARAM lparam,
                            int button,
                            int wheel,
                            HWND hwnd) {
  MouseEvent e;
  e.type = type;
  e.flags = static_cast<int>(wparam);
  e.button = button;
  e.wheel_delta = wheel;
  if (type == MouseEvent::Type::kWheel) {
    POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (hwnd) {
      ScreenToClient(hwnd, &pt);
    }
    e.x = pt.x;
    e.y = pt.y;
  } else {
    e.x = GET_X_LPARAM(lparam);
    e.y = GET_Y_LPARAM(lparam);
  }
  return e;
}

KeyEvent make_key_event(KeyEvent::Type type, WPARAM wparam, LPARAM lparam) {
  KeyEvent e;
  e.type = type;
  e.vk = static_cast<std::uint32_t>(wparam);
  e.flags = static_cast<int>(lparam);
  return e;
}

namespace {

bool read_ime_string(HIMC imc, DWORD index, std::wstring* out) {
  if (!imc || !out) {
    return false;
  }
  const LONG bytes = ImmGetCompositionStringW(imc, index, nullptr, 0);
  if (bytes <= 0) {
    out->clear();
    return true;
  }
  out->assign(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
  ImmGetCompositionStringW(imc, index, out->data(), static_cast<DWORD>(bytes));
  return true;
}

}  // namespace

ImeDispatch dispatch_ime_composition(View* focused, HWND hwnd, LPARAM lparam) {
  if (!focused || !hwnd) {
    return ImeDispatch::kIgnore;
  }
  HIMC imc = ImmGetContext(hwnd);
  if (!imc) {
    return ImeDispatch::kIgnore;
  }
  bool handled = false;
  std::wstring text;
  if (lparam & GCS_RESULTSTR) {
    read_ime_string(imc, GCS_RESULTSTR, &text);
    handled = focused->on_ime_composition(text, true);
  } else if (lparam & GCS_COMPSTR) {
    read_ime_string(imc, GCS_COMPSTR, &text);
    handled = focused->on_ime_composition(text, false);
  }
  ImmReleaseContext(hwnd, imc);
  return handled ? ImeDispatch::kHandled : ImeDispatch::kUnhandled;
}

void Widget::set_focused_view(View* view) {
  if (focused_ == view) {
    return;
  }
  View* previous = focused_;
  focused_ = view;
  if (previous) {
    previous->on_blur();
    previous->invalidate_commands();
  }
  if (focused_) {
    focused_->on_focus();
    focused_->invalidate_commands();
  }
  if (previous && previous->bounds().width > 0 &&
      previous->bounds().height > 0) {
    schedule_paint_rect(previous->bounds());
  }
  if (focused_ && focused_->bounds().width > 0 &&
      focused_->bounds().height > 0) {
    schedule_paint_rect(focused_->bounds());
  }
  if ((!previous || previous->bounds().width <= 0) &&
      (!focused_ || focused_->bounds().width <= 0)) {
    schedule_paint();
  }
}

void Widget::clear_view_refs(View* view) {
  if (focused_ == view) {
    focused_ = nullptr;
  }
  if (hovered_ == view) {
    hovered_ = nullptr;
  }
  if (pressed_ == view) {
    pressed_ = nullptr;
  }
}

bool Widget::advance_focus(bool reverse) {
  if (!contents_) {
    return false;
  }
  std::vector<View*> order;
  collect_focusable_views(contents_.get(), &order);
  if (order.empty()) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < order.size(); ++i) {
    if (order[i] == focused_) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (reverse) {
    index = (index <= 0) ? static_cast<int>(order.size()) - 1 : index - 1;
  } else {
    index = (index + 1) % static_cast<int>(order.size());
  }
  set_focused_view(order[static_cast<size_t>(index)]);
  return true;
}

bool Widget::send_mouse(const MouseEvent& event) {
  if (!contents_) {
    return false;
  }
  View* hit = contents_->get_view_at(event.x, event.y);
  if (event.type == MouseEvent::Type::kMove) {
    update_hover(hit);
  }
  if (event.type == MouseEvent::Type::kDown && event.button == 1) {
    if (pressed_) {
      pressed_->set_pressed(false);
    }
    pressed_ = hit;
    if (pressed_) {
      pressed_->set_pressed(true);
    }
    if (hit && hit->is_focusable() && hit->is_enabled()) {
      hit->request_focus();
    } else {
      set_focused_view(nullptr);
    }
  }
  bool handled = false;
  // Capture: keep move/up on the press target so Splitter drag survives
  // leaving the bar. Down / wheel / right-button go to the hit leaf — same
  // as View::on_mouse_event's get_view_at jump, without a second walk from
  // the root.
  View* target = nullptr;
  if (pressed_ &&
      (event.type == MouseEvent::Type::kMove ||
       (event.type == MouseEvent::Type::kUp && event.button == 1))) {
    target = pressed_;
  } else {
    target = hit;
  }
  if (target) {
    handled = target->on_mouse_event(event);
  }
  // Wheel hits the deepest child (ScrollView content). Leaves often ignore
  // it; walk parents so ScrollView can scroll.
  if (!handled && event.type == MouseEvent::Type::kWheel && hit) {
    for (View* p = hit->parent(); p && !handled; p = p->parent()) {
      handled = p->on_mouse_event(event);
    }
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
    if (pressed_) {
      pressed_->set_pressed(false);
      pressed_ = nullptr;
    }
  }
  return handled;
}

bool Widget::send_key(const KeyEvent& event) {
  if (!contents_) {
    return false;
  }
  if (event.type == KeyEvent::Type::kDown && event.vk == VK_TAB) {
    const bool reverse = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    return advance_focus(reverse);
  }
  if (focused_) {
    return focused_->on_key_event(event);
  }
  return contents_->on_key_event(event);
}

bool Widget::send_char(const CharEvent& event) {
  if (!contents_) {
    return false;
  }
  if (event.ch == L'\t' || event.ch == L'\r' || event.ch == L'\n') {
    return false;
  }
  if (focused_) {
    return focused_->on_char_event(event);
  }
  return contents_->on_char_event(event);
}

bool Widget::dispatch_mouse(MouseEvent::Type type, WPARAM wparam, LPARAM lparam,
                            int button, int wheel) {
  return send_mouse(
      make_mouse_event(type, wparam, lparam, button, wheel, hwnd_));
}

bool Widget::dispatch_key(KeyEvent::Type type, WPARAM wparam, LPARAM lparam) {
  return send_key(make_key_event(type, wparam, lparam));
}

void Widget::track_mouse_leave() {
  if (tracking_leave_ || !hwnd_) {
    return;
  }
  TRACKMOUSEEVENT tme = {};
  tme.cbSize = sizeof(tme);
  tme.dwFlags = TME_LEAVE;
  tme.hwndTrack = hwnd_;
  tracking_leave_ = TrackMouseEvent(&tme) != FALSE;
}

void Widget::update_hover(View* hit) {
  if (hovered_ == hit) {
    return;
  }
  if (hovered_) {
    hovered_->set_hovered(false);
  }
  hovered_ = hit;
  if (hovered_) {
    hovered_->set_hovered(true);
  }
}

}  // namespace views
}  // namespace ui
