// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_HELPERS_H_
#define UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_HELPERS_H_

#include <cstdint>
#include <memory>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/color/color.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/view/view.h"

// Shared expect + input helpers for views_unittests console self-tests.

extern int g_fails;

void expect(bool ok, const char* msg);

ui::views::MouseEvent mouse_up(int x, int y);
ui::views::MouseEvent mouse_down(int x, int y);
ui::views::MouseEvent mouse_move(int x, int y);
ui::views::KeyEvent key_down(std::uint32_t vk);

// Counts paint_self calls for dirty-rect / commit tests.
class PaintProbe : public ui::views::View {
 public:
  int self_paints = 0;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;
};

// Counts layout() calls for device-scale / preferred-size tests.
class CountLayout : public ui::views::LayoutManager {
 public:
  int layouts = 0;
  void layout(ui::views::View*) override;
};

// Offscreen GDI paint of |root| into a temporary DIB (optional clip).
void paint_tree(ui::views::View* root,
                int width,
                int height,
                const ui::views::Rect* clip);

#endif  // UI_VIEWS_TESTING_UNIT_VIEWS_UNIT_HELPERS_H_
