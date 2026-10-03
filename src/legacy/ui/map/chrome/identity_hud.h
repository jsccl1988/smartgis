// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_CHROME_IDENTITY_HUD_H_
#define LEGACY_UI_MAP_CHROME_IDENTITY_HUD_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace detail {

// Top black bar + yellow engine label (matches endgame identity HUD height).
constexpr int kIdentityHudHeight = 28;

// Paint into |hdc|. When |hdc| is null, borrows GetDC(hwnd) and releases it.
void paint_identity_hud(HWND hwnd, HDC hdc, const char* label);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_MAP_CHROME_IDENTITY_HUD_H_
