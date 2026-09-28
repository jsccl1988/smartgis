// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_PAINT_PAINT_COMMIT_H_
#define UI_VIEWS_KERNEL_PAINT_PAINT_COMMIT_H_

#include "ui/ui_views_export.h"
#include <cstdint>

#include "ui/gfx/color/color.h"
#include "ui/gfx/display_list/display_list.h"
#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Immutable shell paint snapshot. Produced on the UI thread by copying
// View DisplayLists so later mutations of View::commands_ cannot affect an
// in-flight raster. Worker threads only read this object after Activate.
struct PaintCommit {
  ui::gfx::DisplayList display_list;
  Rect dirty;
  int width_px = 0;
  int height_px = 0;
  int font_px = 12;
  ui::gfx::Color clear_color = 0;
  std::uint64_t generation = 0;
};

// Records dirty View DisplayLists on the calling thread (thread_local
// recorder), then copies commands into |out|. Returns false if arguments are
// invalid. Does not touch HWND or present.
UI_VIEWS_EXPORT bool commit_view_tree(View* root,
                      const Rect& dirty,
                      int width_px,
                      int height_px,
                      int font_px,
                      ui::gfx::Color clear_color,
                      PaintCommit* out);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_PAINT_PAINT_COMMIT_H_
