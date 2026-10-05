// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/shell/theme.h"

#include <dwrite.h>

#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme_service.h"

#include <mutex>

namespace ui {
namespace views {

const Theme& Theme::current() {
  return ThemeService::get().theme();
}

std::wstring utf8_to_wide(const std::string& u8) {
  if (u8.empty()) {
    return L"";
  }
  ui::gfx::note_utf8_conversion();
  const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
  if (n <= 0) {
    return L"";
  }
  // n includes the trailing NUL; the string size must too so the convert
  // cannot write past SSO/heap (Debug CRT 0xC0000374 on the next alloc).
  std::wstring w(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), n);
  w.resize(static_cast<size_t>(n - 1));
  return w;
}

std::string wide_to_utf8(const wchar_t* w) {
  if (!w || !w[0]) {
    return {};
  }
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0) {
    return {};
  }
  std::string s(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  s.resize(static_cast<size_t>(n - 1));
  return s;
}

Size measure_text_utf8(const std::string& text) {
  return measure_text_utf8(text, 1.f);
}

namespace {

struct FontSlot {
  int px = 0;
  HFONT font = nullptr;
};

FontSlot g_fonts[8];
int g_font_count = 0;
IDWriteFactory* g_dwrite = nullptr;
bool g_dwrite_tried = false;
IDWriteTextFormat* g_formats[8] = {};
int g_format_px[8] = {};
int g_format_count = 0;
std::mutex g_type_mu;

HFONT font_for_px(int px) {
  if (px < 1) {
    px = 1;
  }
  for (int i = 0; i < g_font_count; ++i) {
    if (g_fonts[i].px == px && g_fonts[i].font) {
      return g_fonts[i].font;
    }
  }
  ui::gfx::note_create_font();
  HFONT font = CreateFontW(
      -px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  if (font && g_font_count < 8) {
    g_fonts[g_font_count].px = px;
    g_fonts[g_font_count].font = font;
    ++g_font_count;
  }
  return font;
}

bool ensure_dwrite() {
  if (g_dwrite_tried) {
    return g_dwrite != nullptr;
  }
  g_dwrite_tried = true;
  const HRESULT hr = DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown**>(&g_dwrite));
  if (FAILED(hr)) {
    g_dwrite = nullptr;
  }
  return g_dwrite != nullptr;
}

IDWriteTextFormat* format_for_px(int px) {
  if (!ensure_dwrite()) {
    return nullptr;
  }
  for (int i = 0; i < g_format_count; ++i) {
    if (g_format_px[i] == px) {
      return g_formats[i];
    }
  }
  if (g_format_count >= 8) {
    return g_formats[0];
  }
  IDWriteTextFormat* format = nullptr;
  const HRESULT hr = g_dwrite->CreateTextFormat(
      L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, static_cast<FLOAT>(px), L"en-us", &format);
  if (FAILED(hr) || !format) {
    return nullptr;
  }
  g_formats[g_format_count] = format;
  g_format_px[g_format_count] = px;
  ++g_format_count;
  return format;
}

Size gdi_extent(const std::wstring& text, int px) {
  Size out;
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return out;
  }
  HFONT font = font_for_px(px);
  HGDIOBJ old = font ? SelectObject(screen, font) : nullptr;
  SIZE sz = {};
  if (GetTextExtentPoint32W(screen, text.c_str(),
                           static_cast<int>(text.size()), &sz)) {
    out.width = sz.cx;
    out.height = sz.cy;
  }
  if (font) {
    SelectObject(screen, old);
  }
  ReleaseDC(nullptr, screen);
  return out;
}

// DWrite fallback when GDI cannot measure. Do not keep a process-global
// IDWriteTextLayout cache: the previous 48-slot shift-eviction shallow-copied
// COM pointers and, together with a GDI-success "warmup" CreateTextLayout on
// every Label/Button measure during shell BuildContents, corrupted the CRT
// heap (STATUS_HEAP_CORRUPTION / RtlReportCriticalFailure at the next alloc
// inside DWrite::CreateTextLayout — FeatureInfo::refresh_frame).
Size dwrite_extent(const std::wstring& wide, int face_px) {
  Size out;
  if (!ensure_dwrite()) {
    return out;
  }
  IDWriteTextFormat* format = format_for_px(face_px);
  if (!format || !g_dwrite) {
    return out;
  }
  IDWriteTextLayout* layout = nullptr;
  const HRESULT hr = g_dwrite->CreateTextLayout(
      wide.c_str(), static_cast<UINT32>(wide.size()), format, 10000.f, 1000.f,
      &layout);
  if (FAILED(hr) || !layout) {
    return out;
  }
  DWRITE_TEXT_METRICS metrics = {};
  if (SUCCEEDED(layout->GetMetrics(&metrics))) {
    out.width = static_cast<int>(metrics.width + 0.5f);
    out.height = static_cast<int>(metrics.height + 0.5f);
  }
  layout->Release();
  return out;
}

Size measure_cached(const std::wstring& wide, int face_px) {
  std::lock_guard<std::mutex> lock(g_type_mu);
  Size out = gdi_extent(wide, face_px);
  if (out.width > 0 || out.height > 0) {
    return out;
  }
  return dwrite_extent(wide, face_px);
}

}  // namespace

Size measure_text_utf8(const std::string& text, float device_scale) {
  Size out;
  if (text.empty()) {
    return out;
  }
  ui::gfx::note_measure_text();
  if (device_scale <= 0.f) {
    device_scale = 1.f;
  }
  const int px = shell_body_font_px(device_scale);
  const std::wstring wide = utf8_to_wide(text);
  const int face_px = px > 0 ? px : kShellBodyFontDip;
  return measure_cached(wide, face_px);
}

void draw_focus_ring(ui::gfx::Canvas* canvas, const Rect& bounds) {
  if (!canvas) {
    return;
  }
  canvas->stroke_rect(bounds.x, bounds.y, bounds.width, bounds.height,
                      Theme::current().accent, 1);
}

}  // namespace views
}  // namespace ui
