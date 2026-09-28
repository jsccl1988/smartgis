// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_

#include "ui/ui_export.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Tab host. add_tab takes ownership of the page View. Click a tab to
// show that page; only the active page is laid out and painted.
class UI_EXPORT TabStrip : public View {
 public:
  TabStrip();
  int add_tab(std::string title, std::unique_ptr<View> page);
  void set_active(int i);
  int active() const;
  View* page_at(int i) const;
  int tab_count() const;
  void set_change(std::function<void(int)> fn);
  bool on_mouse_event(const MouseEvent& e) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  void layout() override;
  std::string_view paint_role() const override;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  void apply_page_visibility();
  int tab_at(int x, int y) const;
  int tab_height() const;

  std::vector<std::string> titles_;
  std::vector<View*> pages_;
  int active_ = -1;
  std::function<void(int)> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_
