// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_EAGLE_EYE_PERSIST_H_
#define UI_VIEWS_MAP_EAGLE_EYE_PERSIST_H_

namespace ui {
namespace views {
namespace detail {

// %LocalAppData%/SmartGIS/eagle_eye_visible.txt — default visible when missing.
bool eagle_eye_load_visible();
void eagle_eye_persist_visible(bool visible);

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_EAGLE_EYE_PERSIST_H_
