// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_INSPECTOR_GROUP_BAR_H_
#define APP_VIEWS_UI_HORIZON_INSPECTOR_GROUP_BAR_H_

#include <functional>

#include "ui/views/kernel/view/view.h"

namespace app {

// Segmented selector above the inspector TabStrip. Four workflow groups keep
// the tab titles readable inside the dock width.
class InspectorGroupBar : public ui::views::View {
 public:
  enum class Group {
    kInspect = 0,
    kMap,
    kProcess,
    kExtend,
    kCount,
  };

  InspectorGroupBar();
  void set_selected(Group group);
  Group selected() const { return selected_; }
  void set_change(std::function<void(Group)> fn);
  bool on_mouse_event(const ui::views::MouseEvent& e) override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  Group selected_ = Group::kMap;
  std::function<void(Group)> change_;
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_INSPECTOR_GROUP_BAR_H_
