// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_FACTORY_PLACEHOLDER_VIEW_H_
#define UI_VIEWS_MARKUP_FACTORY_PLACEHOLDER_VIEW_H_

#include "ui/ui_export.h"
#include <string>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// GIS / unknown-tag stub: dashed box with a caption (id or tag).
class UI_EXPORT PlaceholderView : public View {
 public:
  explicit PlaceholderView(std::string caption);

  const std::string& caption() const { return caption_; }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  std::string caption_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_FACTORY_PLACEHOLDER_VIEW_H_
