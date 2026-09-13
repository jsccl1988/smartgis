// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_LABEL_H_
#define UI_VIEWS_LABEL_H_

#include <string>

#include "render/skia/color.h"
#include "ui/views/view.h"

namespace ui {
namespace views {

// Static text node. Color defaults to the current Theme text.
class Label : public View {
 public:
  explicit Label(std::string text);
  void set_text(std::string text);
  const std::string& text() const;
  void set_color(render::skia::Color color);
  void clear_color();

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  std::string text_;
  render::skia::Color color_ = 0;
  bool has_color_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_LABEL_H_
