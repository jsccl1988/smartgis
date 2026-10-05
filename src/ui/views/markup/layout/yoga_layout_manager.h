// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_
#define UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_

#include "ui/ui_export.h"
#include <unordered_map>
#include <vector>

#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/style/flex_style.h"

namespace ui {
namespace views {

// LayoutManager backed by Yoga (W3C Flexbox). Positions direct children of the
// host View; nested hosts may each own their own YogaLayoutManager. Unchanged
// host bounds / styles / child measure inputs skip YGNodeCalculateLayout.
class UI_EXPORT YogaLayoutManager : public LayoutCrtp<YogaLayoutManager> {
 public:
  YogaLayoutManager();
  ~YogaLayoutManager() override;

  YogaLayoutManager(const YogaLayoutManager&) = delete;
  YogaLayoutManager& operator=(const YogaLayoutManager&) = delete;

  void set_host_style(const FlexStyle& style);
  const FlexStyle& host_style() const { return host_style_; }

  void set_child_style(const View* child, const FlexStyle& style);
  void clear_child_style(const View* child);

  void layout(View* host) final;
  Size get_preferred_size(const View* host) const final;

  friend class LayoutCrtp<YogaLayoutManager>;

 private:
  void layout_impl(View* host);
  Size preferred_impl(const View* host) const;

  // Snapshot of direct children that Yoga consumes (visibility + measured
  // preferred size). Host bounds sit beside this; style edits drop the cache.
  struct ChildSnap {
    const View* child = nullptr;
    bool visible = false;
    int pref_w = 0;
    int pref_h = 0;
  };

  std::vector<ChildSnap> capture_child_snaps(const View* host) const;
  bool child_snaps_equal(const std::vector<ChildSnap>& a,
                         const std::vector<ChildSnap>& b) const;
  bool flex_layout_equal(const FlexStyle& a, const FlexStyle& b) const;
  void invalidate_yoga_cache();

  FlexStyle host_style_;
  std::unordered_map<const View*, FlexStyle> child_styles_;

  const View* layout_host_ = nullptr;
  Rect layout_host_bounds_{};
  std::vector<ChildSnap> layout_snaps_;
  bool layout_valid_ = false;

  mutable const View* pref_host_ = nullptr;
  mutable std::vector<ChildSnap> pref_snaps_;
  mutable Size pref_size_{};
  mutable bool pref_valid_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_
