// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_FONT_FONT_H_
#define UI_GFX_FONT_FONT_H_

#include "ui/ui_views_export.h"
#include <string>

#include "ui/gfx/geometry/size.h"

namespace ui {
namespace gfx {

// Shell typeface description. Canvas backends may ignore fields they cannot
// apply; this is not a full Chromium FontList / RenderText stack.
struct Font {
  std::wstring family = L"Segoe UI";
  int size_px = 12;
  bool bold = false;
};

// Approximate ink size for |text| with |font|. GDI path uses a temporary HFONT
// when an HDC is available via CreateCompatibleDC; otherwise returns a
// character-count estimate. Empty / null text → {0,0}.
UI_VIEWS_EXPORT Size measure_text_with_font(const Font& font, const wchar_t* text);

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_FONT_FONT_H_
