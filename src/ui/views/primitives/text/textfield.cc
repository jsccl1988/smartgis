// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/primitives/text/textfield.h"

#include <algorithm>
#include <cstring>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {
namespace {

constexpr UINT_PTR kCaretBlinkTimerId = 0x43415245u;  // 'CARE'
constexpr UINT kCaretBlinkMs = 500;

float scale_for(const View* view) {
  if (view && view->widget()) {
    return view->widget()->device_scale_factor();
  }
  return 1.f;
}

bool ctrl_down() {
  return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool shift_down() {
  return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}

// Walk UTF-8 by code points (not grapheme clusters).
size_t utf8_prev(const std::string& s, size_t i) {
  if (i == 0 || i > s.size()) {
    return 0;
  }
  do {
    --i;
  } while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80);
  return i;
}

size_t utf8_next(const std::string& s, size_t i) {
  if (i >= s.size()) {
    return s.size();
  }
  const unsigned char c = static_cast<unsigned char>(s[i]);
  size_t n = 1;
  if ((c & 0x80) == 0) {
    n = 1;
  } else if ((c & 0xE0) == 0xC0) {
    n = 2;
  } else if ((c & 0xF0) == 0xE0) {
    n = 3;
  } else if ((c & 0xF8) == 0xF0) {
    n = 4;
  }
  if (i + n > s.size()) {
    return s.size();
  }
  return i + n;
}

size_t clamp_utf8_offset(const std::string& s, size_t i) {
  if (i > s.size()) {
    return s.size();
  }
  while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
    --i;
  }
  return i;
}

std::string utf8_prefix(const std::string& s, size_t end) {
  end = clamp_utf8_offset(s, end);
  return s.substr(0, end);
}

bool clipboard_set_utf8(HWND owner, const std::string& utf8) {
  if (!OpenClipboard(owner)) {
    return false;
  }
  EmptyClipboard();
  const std::wstring wide = utf8_to_wide(utf8);
  const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) {
    CloseClipboard();
    return false;
  }
  void* locked = GlobalLock(mem);
  if (!locked) {
    GlobalFree(mem);
    CloseClipboard();
    return false;
  }
  std::memcpy(locked, wide.c_str(), bytes);
  GlobalUnlock(mem);
  SetClipboardData(CF_UNICODETEXT, mem);
  CloseClipboard();
  return true;
}

bool clipboard_get_utf8(HWND owner, std::string* out) {
  if (!out || !IsClipboardFormatAvailable(CF_UNICODETEXT) ||
      !OpenClipboard(owner)) {
    return false;
  }
  HANDLE data = GetClipboardData(CF_UNICODETEXT);
  if (!data) {
    CloseClipboard();
    return false;
  }
  const wchar_t* locked = static_cast<const wchar_t*>(GlobalLock(data));
  if (!locked) {
    CloseClipboard();
    return false;
  }
  *out = wide_to_utf8(locked);
  GlobalUnlock(data);
  CloseClipboard();
  // Strip CR/LF for single-line field.
  out->erase(std::remove(out->begin(), out->end(), '\r'), out->end());
  out->erase(std::remove(out->begin(), out->end(), '\n'), out->end());
  return true;
}

}  // namespace

Textfield::Textfield() {
  set_preferred_size({200, 28});
  set_focusable(true);
}

Textfield::~Textfield() {
  disarm_caret_timer();
}

void Textfield::set_text(std::string text) {
  if (text_ == text) {
    return;
  }
  text_ = std::move(text);
  caret_ = text_.size();
  sel_anchor_ = caret_;
  composition_.clear();
  schedule_paint();
}

const std::string& Textfield::text() const {
  return text_;
}

void Textfield::set_placeholder(std::string placeholder) {
  if (placeholder_ == placeholder) {
    return;
  }
  placeholder_ = std::move(placeholder);
  schedule_paint();
}

void Textfield::set_invalid(bool invalid) {
  if (invalid_ == invalid) {
    return;
  }
  invalid_ = invalid;
  schedule_paint();
}

void Textfield::set_change(std::function<void()> fn) {
  change_ = std::move(fn);
}

void Textfield::set_submit(std::function<void()> fn) {
  submit_ = std::move(fn);
}

void Textfield::set_key_hook(std::function<bool(const KeyEvent&)> fn) {
  key_hook_ = std::move(fn);
}

void Textfield::notify_change() {
  schedule_paint();
  if (change_) {
    change_();
  }
}

bool Textfield::has_selection() const {
  return sel_anchor_ != caret_;
}

size_t Textfield::selection_begin() const {
  return std::min(sel_anchor_, caret_);
}

size_t Textfield::selection_end() const {
  return std::max(sel_anchor_, caret_);
}

void Textfield::clear_selection() {
  sel_anchor_ = caret_;
}

void Textfield::set_caret(size_t utf8_offset, bool extend_selection) {
  caret_ = clamp_utf8_offset(text_, utf8_offset);
  if (!extend_selection) {
    sel_anchor_ = caret_;
  }
  schedule_paint();
}

void Textfield::delete_selection() {
  if (!has_selection()) {
    return;
  }
  const size_t begin = selection_begin();
  const size_t end = selection_end();
  text_.erase(begin, end - begin);
  caret_ = begin;
  sel_anchor_ = caret_;
  notify_change();
}

void Textfield::insert_utf8(std::string_view utf8) {
  if (utf8.empty()) {
    return;
  }
  if (has_selection()) {
    delete_selection();
  }
  text_.insert(caret_, utf8);
  caret_ += utf8.size();
  sel_anchor_ = caret_;
  notify_change();
}

bool Textfield::delete_backward() {
  if (has_selection()) {
    delete_selection();
    return true;
  }
  if (caret_ == 0) {
    return true;
  }
  const size_t prev = utf8_prev(text_, caret_);
  text_.erase(prev, caret_ - prev);
  caret_ = prev;
  sel_anchor_ = caret_;
  notify_change();
  return true;
}

bool Textfield::delete_forward() {
  if (has_selection()) {
    delete_selection();
    return true;
  }
  if (caret_ >= text_.size()) {
    return true;
  }
  const size_t next = utf8_next(text_, caret_);
  text_.erase(caret_, next - caret_);
  sel_anchor_ = caret_;
  notify_change();
  return true;
}

void Textfield::move_caret(int utf8_delta, bool extend_selection) {
  size_t pos = caret_;
  if (utf8_delta < 0) {
    for (int i = 0; i < -utf8_delta && pos > 0; ++i) {
      pos = utf8_prev(text_, pos);
    }
  } else {
    for (int i = 0; i < utf8_delta && pos < text_.size(); ++i) {
      pos = utf8_next(text_, pos);
    }
  }
  set_caret(pos, extend_selection);
}

void Textfield::select_all() {
  sel_anchor_ = 0;
  caret_ = text_.size();
  schedule_paint();
}

bool Textfield::copy_selection_to_clipboard() const {
  if (!has_selection()) {
    return false;
  }
  HWND owner = widget() ? widget()->hwnd() : nullptr;
  const size_t begin = selection_begin();
  const size_t end = selection_end();
  return clipboard_set_utf8(owner, text_.substr(begin, end - begin));
}

bool Textfield::cut_selection_to_clipboard() {
  if (!copy_selection_to_clipboard()) {
    return false;
  }
  delete_selection();
  return true;
}

bool Textfield::paste_from_clipboard() {
  std::string clip;
  HWND owner = widget() ? widget()->hwnd() : nullptr;
  if (!clipboard_get_utf8(owner, &clip) || clip.empty()) {
    return false;
  }
  insert_utf8(clip);
  return true;
}

size_t Textfield::caret_from_x(int client_x) const {
  const float scale = scale_for(this);
  const int pad_x = dip_to_px(8, scale);
  const int target = client_x - bounds().x - pad_x;
  if (target <= 0 || text_.empty()) {
    return 0;
  }
  size_t best = 0;
  int best_dist = target;
  size_t i = 0;
  while (i <= text_.size()) {
    const int w = measure_text_utf8(utf8_prefix(text_, i), scale).width;
    const int dist = std::abs(w - target);
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
    if (i >= text_.size()) {
      break;
    }
    i = utf8_next(text_, i);
  }
  return best;
}

void Textfield::arm_caret_timer() {
  if (!widget() || !widget()->hwnd()) {
    return;
  }
  SetTimer(widget()->hwnd(), kCaretBlinkTimerId, kCaretBlinkMs, nullptr);
}

void Textfield::disarm_caret_timer() {
  if (!widget() || !widget()->hwnd()) {
    return;
  }
  KillTimer(widget()->hwnd(), kCaretBlinkTimerId);
}

void Textfield::on_focus() {
  arm_caret_timer();
  schedule_paint();
}

void Textfield::on_blur() {
  disarm_caret_timer();
  composition_.clear();
  selecting_drag_ = false;
  schedule_paint();
}

bool Textfield::on_ime_composition(std::wstring_view text, bool is_result) {
  if (!is_enabled() || !is_focused()) {
    return false;
  }
  if (is_result) {
    composition_.clear();
    if (!text.empty()) {
      insert_utf8(wide_to_utf8(std::wstring(text).c_str()));
    } else {
      schedule_paint();
    }
    return true;
  }
  composition_.assign(text.begin(), text.end());
  schedule_paint();
  return true;
}

bool Textfield::on_mouse_event(const MouseEvent& e) {
  if (!is_enabled()) {
    return false;
  }
  if (e.type == MouseEvent::Type::kDown && e.button == 1) {
    request_focus();
    const size_t pos = caret_from_x(e.x);
    set_caret(pos, shift_down());
    selecting_drag_ = true;
    return true;
  }
  if (e.type == MouseEvent::Type::kDblClick && e.button == 1) {
    select_all();
    return true;
  }
  if (selecting_drag_ && e.type == MouseEvent::Type::kMove) {
    set_caret(caret_from_x(e.x), true);
    return true;
  }
  if (e.type == MouseEvent::Type::kUp && e.button == 1) {
    selecting_drag_ = false;
    return true;
  }
  return false;
}

bool Textfield::on_key_event(const KeyEvent& e) {
  if (!is_enabled() || !is_focused() || e.type != KeyEvent::Type::kDown) {
    return false;
  }
  if (key_hook_ && key_hook_(e)) {
    return true;
  }
  const bool extend = shift_down();
  if (ctrl_down()) {
    switch (e.vk) {
      case 'A':
        select_all();
        return true;
      case 'C':
        copy_selection_to_clipboard();
        return true;
      case 'X':
        cut_selection_to_clipboard();
        return true;
      case 'V':
        paste_from_clipboard();
        return true;
      default:
        break;
    }
  }
  switch (e.vk) {
    case VK_BACK:
      return delete_backward();
    case VK_DELETE:
      return delete_forward();
    case VK_LEFT:
      if (has_selection() && !extend) {
        set_caret(selection_begin(), false);
      } else {
        move_caret(-1, extend);
      }
      return true;
    case VK_RIGHT:
      if (has_selection() && !extend) {
        set_caret(selection_end(), false);
      } else {
        move_caret(1, extend);
      }
      return true;
    case VK_HOME:
      set_caret(0, extend);
      return true;
    case VK_END:
      set_caret(text_.size(), extend);
      return true;
    case VK_RETURN:
      if (submit_) {
        submit_();
      }
      return true;
    default:
      break;
  }
  return false;
}

bool Textfield::on_char_event(const CharEvent& e) {
  if (!is_enabled() || !is_focused()) {
    return false;
  }
  // Ctrl shortcuts arrive as low control chars; skip them.
  if (ctrl_down() || e.ch < 32 || e.ch == 127) {
    return e.ch == L'\b' ? delete_backward() : false;
  }
  if (e.ch == L'\b') {
    return delete_backward();
  }
  if (!composition_.empty()) {
    // IME result usually arrives via on_ime_composition; ignore stray CHAR.
    return true;
  }
  const wchar_t buf[2] = {e.ch, 0};
  insert_utf8(wide_to_utf8(buf));
  return true;
}

void Textfield::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = scale_for(this);
  const int pad_x = dip_to_px(8, scale);

  canvas->fill_rect(b.x, b.y, b.width, b.height,
                    is_enabled() ? t.control_bg : t.control_disabled);

  ui::gfx::Color edge = t.control_border;
  if (invalid_) {
    edge = t.danger;
  } else if (is_focused()) {
    edge = t.accent;
  } else if (is_hovered()) {
    edge = t.control_hover;
  }
  canvas->stroke_rect(b.x, b.y, b.width, b.height, edge, 1);

  const bool show_placeholder =
      text_.empty() && composition_.empty() && !placeholder_.empty();
  const std::string display_utf8 =
      show_placeholder ? placeholder_
                       : (composition_.empty()
                              ? text_
                              : text_.substr(0, caret_) +
                                    wide_to_utf8(composition_.c_str()) +
                                    text_.substr(caret_));
  const ui::gfx::Color fg = !is_enabled()
                                ? t.text_muted
                                : (show_placeholder ? t.text_muted : t.text);

  const Size ink = measure_text_utf8(
      show_placeholder ? placeholder_ : (composition_.empty() ? text_ : display_utf8),
      scale);
  int text_y = b.y + dip_to_px(4, scale);
  if (ink.height > 0 && b.height > ink.height) {
    text_y = b.y + (b.height - ink.height) / 2;
  }
  const int text_x = b.x + pad_x;

  if (is_focused() && has_selection() && !show_placeholder) {
    const size_t begin = selection_begin();
    const size_t end = selection_end();
    const int x0 = text_x + measure_text_utf8(utf8_prefix(text_, begin), scale).width;
    const int x1 = text_x + measure_text_utf8(utf8_prefix(text_, end), scale).width;
    const int sel_h = ink.height > 0 ? ink.height : shell_body_font_px(scale);
    canvas->fill_rect(x0, text_y, std::max(1, x1 - x0), sel_h, t.accent);
  }

  if (!display_utf8.empty()) {
    canvas->draw_text(text_x, text_y, utf8_to_wide(display_utf8).c_str(), fg);
  }

  // Underline live IME composition (industry text-control cue).
  if (is_focused() && !composition_.empty()) {
    const int pre_w =
        measure_text_utf8(utf8_prefix(text_, caret_), scale).width;
    const int comp_w =
        measure_text_utf8(wide_to_utf8(composition_.c_str()), scale).width;
    const int y = text_y + (ink.height > 0 ? ink.height : shell_body_font_px(scale));
    canvas->fill_rect(text_x + pre_w, y, std::max(1, comp_w), 1, t.accent);
  }

  if (is_focused() && !show_placeholder && composition_.empty()) {
    const bool blink_on = ((GetTickCount() / kCaretBlinkMs) % 2) == 0;
    if (blink_on) {
      const int caret_x =
          text_x + measure_text_utf8(utf8_prefix(text_, caret_), scale).width;
      const int caret_h =
          ink.height > 0 ? ink.height : shell_body_font_px(scale);
      canvas->fill_rect(caret_x, text_y, std::max(1, dip_to_px(1, scale)),
                        caret_h, t.text_bright);
    }
  }

  if (is_focused() && !invalid_) {
    draw_focus_ring(canvas, b);
  }
}

std::string_view Textfield::paint_role() const {
  return "textfield";
}
}  // namespace views
}  // namespace ui
