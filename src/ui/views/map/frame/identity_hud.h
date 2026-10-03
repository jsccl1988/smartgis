// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_IDENTITY_HUD_H_
#define UI_VIEWS_MAP_IDENTITY_HUD_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace detail {

// Legacy-matching top HUD: black bar + yellow engine name + Fps.
extern const wchar_t kIdentityHudClass[];
constexpr int kIdentityHudHeight = 28;

void register_identity_hud_class();

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_IDENTITY_HUD_H_
