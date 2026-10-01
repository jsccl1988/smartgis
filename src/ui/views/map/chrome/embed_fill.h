// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_CHROME_EMBED_FILL_H_
#define UI_VIEWS_MAP_CHROME_EMBED_FILL_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ui {
namespace views {
namespace detail {

// Opaque fill for the embed HWND client. Parent WS_CLIPCHILDREN + a skipped
// FillRect leaves a desktop / Cursor see-through hole (NULL_BRUSH era).
void fill_map_embed_opaque(HDC hdc, const RECT& rc, bool scene3d);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_CHROME_EMBED_FILL_H_
