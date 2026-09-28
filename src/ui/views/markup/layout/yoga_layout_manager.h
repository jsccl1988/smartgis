// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_
#define UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_

#include "ui/ui_export.h"
#include <memory>
#include <unordered_map>

#include "ui/views/kernel/layout/layout.h"
#include "ui/views/markup/style/flex_style.h"

namespace ui {
namespace views {

// LayoutManager backed by Yoga (W3C Flexbox). Positions direct children of the
// host View; nested hosts may each own their own YogaLayoutManager.
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

  FlexStyle host_style_;
  std::unordered_map<const View*, FlexStyle> child_styles_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MARKUP_LAYOUT_YOGA_LAYOUT_MANAGER_H_
