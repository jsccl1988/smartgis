// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_
#define UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_

#include "ui/ui_export.h"
#include <cstdint>
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
  // Where the clickable title band sits relative to the page body.
  enum class HeaderPlacement {
    kTop,
    kBottom,
  };

  TabStrip();
  int add_tab(std::string title, std::unique_ptr<View> page);
  // Replace the page View at |i| (keeps title). Returns false if index invalid.
  bool replace_page(int i, std::unique_ptr<View> page);
  void set_active(int i);
  int active() const;
  View* page_at(int i) const;
  int tab_count() const;
  void set_change(std::function<void(int)> fn);
  void set_header_placement(HeaderPlacement placement);
  HeaderPlacement header_placement() const { return header_placement_; }
  bool on_mouse_event(const MouseEvent& e) override;
  void on_device_scale_factor_changed(float old_scale,
                                     float new_scale) override;
  void layout() override;
  std::string_view paint_role() const override;

  // Header strip height in device pixels (DIP-scaled).
  int tab_height() const;
  // Absolute bounds of the clickable tab header band.
  Rect header_bounds() const;
  // Content-sized tab cell (label + pad), packed left-to-right.
  // Mild overflow shrinks toward a readable floor. When the floor still does
  // not fit, extra tabs move into the overflow menu (width 0 here).
  int tab_width_at(int i) const;
  int tab_x_at(int i) const;
  // Tabs omitted from the header (another inspector group). Included by default.
  void set_tab_included(int i, bool included);
  bool tab_included(int i) const;
  bool has_tab_overflow() const;
  int overflow_count() const;
  int overflow_tab_at(int i) const;

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override;

 private:
  static constexpr int kMaxTabs = 32;

  struct SlotLayout {
    int width[kMaxTabs] = {};
    int chevron_w = 0;
    int overflow[kMaxTabs] = {};
    int overflow_n = 0;
  };

  void apply_page_visibility();
  int tab_at(int x, int y) const;
  int natural_tab_width_at(int i) const;
  void layout_slots(SlotLayout* out) const;
  void open_overflow_menu();

  std::vector<std::string> titles_;
  std::vector<View*> pages_;
  // 1 = shown in the header. Fixed capacity so the callback member stays put.
  uint8_t included_[kMaxTabs] = {};
  int active_ = -1;
  HeaderPlacement header_placement_ = HeaderPlacement::kTop;
  std::function<void(int)> change_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_PRIMITIVES_COLLECTION_TAB_STRIP_H_
