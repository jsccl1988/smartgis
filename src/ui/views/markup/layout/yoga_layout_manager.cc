// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/markup/layout/yoga_layout_manager.h"

#include <cmath>
#include <vector>

#include <yoga/Yoga.h>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {
namespace {

YGFlexDirection to_yg_direction(FlexStyle::FlexDirection d) {
  return d == FlexStyle::FlexDirection::kRow ? YGFlexDirectionRow
                                             : YGFlexDirectionColumn;
}

YGJustify to_yg_justify(FlexStyle::Justify j) {
  switch (j) {
    case FlexStyle::Justify::kFlexEnd:
      return YGJustifyFlexEnd;
    case FlexStyle::Justify::kCenter:
      return YGJustifyCenter;
    case FlexStyle::Justify::kSpaceBetween:
      return YGJustifySpaceBetween;
    case FlexStyle::Justify::kSpaceAround:
      return YGJustifySpaceAround;
    case FlexStyle::Justify::kSpaceEvenly:
      return YGJustifySpaceEvenly;
    case FlexStyle::Justify::kFlexStart:
    default:
      return YGJustifyFlexStart;
  }
}

YGAlign to_yg_align(FlexStyle::Align a) {
  switch (a) {
    case FlexStyle::Align::kFlexStart:
      return YGAlignFlexStart;
    case FlexStyle::Align::kFlexEnd:
      return YGAlignFlexEnd;
    case FlexStyle::Align::kCenter:
      return YGAlignCenter;
    case FlexStyle::Align::kStretch:
      return YGAlignStretch;
    case FlexStyle::Align::kAuto:
    default:
      return YGAlignAuto;
  }
}

void apply_flex_style(YGNodeRef node, const FlexStyle& style, bool is_host) {
  if (style.display.has_value()) {
    YGNodeStyleSetDisplay(
        node, *style.display == FlexStyle::Display::kNone ? YGDisplayNone
                                                          : YGDisplayFlex);
  }

  if (is_host) {
    if (style.flex_direction.has_value()) {
      YGNodeStyleSetFlexDirection(node, to_yg_direction(*style.flex_direction));
    }
    if (style.justify_content.has_value()) {
      YGNodeStyleSetJustifyContent(node, to_yg_justify(*style.justify_content));
    }
    if (style.align_items.has_value()) {
      YGNodeStyleSetAlignItems(node, to_yg_align(*style.align_items));
    } else {
      YGNodeStyleSetAlignItems(node, YGAlignStretch);
    }
    if (style.gap.has_value()) {
      YGNodeStyleSetGap(node, YGGutterAll, *style.gap);
    }
    const float pad = style.padding.value_or(0.f);
    YGNodeStyleSetPadding(node, YGEdgeLeft,
                          style.padding_left.value_or(pad));
    YGNodeStyleSetPadding(node, YGEdgeTop, style.padding_top.value_or(pad));
    YGNodeStyleSetPadding(node, YGEdgeRight,
                          style.padding_right.value_or(pad));
    YGNodeStyleSetPadding(node, YGEdgeBottom,
                          style.padding_bottom.value_or(pad));
  }

  if (style.flex_grow.has_value()) {
    YGNodeStyleSetFlexGrow(node, *style.flex_grow);
  } else if (style.flex.has_value()) {
    YGNodeStyleSetFlexGrow(node, *style.flex);
  }
  if (style.flex_shrink.has_value()) {
    YGNodeStyleSetFlexShrink(node, *style.flex_shrink);
  }
  if (style.flex.has_value()) {
    YGNodeStyleSetFlex(node, *style.flex);
  }

  const float margin = style.margin.value_or(0.f);
  YGNodeStyleSetMargin(node, YGEdgeLeft, style.margin_left.value_or(margin));
  YGNodeStyleSetMargin(node, YGEdgeTop, style.margin_top.value_or(margin));
  YGNodeStyleSetMargin(node, YGEdgeRight, style.margin_right.value_or(margin));
  YGNodeStyleSetMargin(node, YGEdgeBottom,
                       style.margin_bottom.value_or(margin));

  if (style.width.has_value()) {
    YGNodeStyleSetWidth(node, *style.width);
  }
  if (style.height.has_value()) {
    YGNodeStyleSetHeight(node, *style.height);
  }
  if (style.min_width.has_value()) {
    YGNodeStyleSetMinWidth(node, *style.min_width);
  }
  if (style.min_height.has_value()) {
    YGNodeStyleSetMinHeight(node, *style.min_height);
  }
  if (style.max_width.has_value()) {
    YGNodeStyleSetMaxWidth(node, *style.max_width);
  }
  if (style.max_height.has_value()) {
    YGNodeStyleSetMaxHeight(node, *style.max_height);
  }
}

YGSize measure_view(YGNodeConstRef node,
                    float /*width*/,
                    YGMeasureMode /*widthMode*/,
                    float /*height*/,
                    YGMeasureMode /*heightMode*/) {
  auto* view = static_cast<View*>(YGNodeGetContext(node));
  YGSize out{0.f, 0.f};
  if (!view) {
    return out;
  }
  const Size pref = view->get_preferred_size();
  out.width = static_cast<float>(pref.width);
  out.height = static_cast<float>(pref.height);
  return out;
}

int round_px(float v) {
  if (!std::isfinite(v)) {
    return 0;
  }
  return static_cast<int>(std::lround(v));
}

struct YogaTree {
  YGNodeRef root = nullptr;
  std::vector<YGNodeRef> children;

  ~YogaTree() {
    if (root) {
      YGNodeFreeRecursive(root);
      root = nullptr;
      children.clear();
    }
  }

  YogaTree() = default;
  YogaTree(const YogaTree&) = delete;
  YogaTree& operator=(const YogaTree&) = delete;
};

const FlexStyle* find_child_style(
    const std::unordered_map<const View*, FlexStyle>& map,
    const View* child) {
  const auto it = map.find(child);
  return it == map.end() ? nullptr : &it->second;
}

void build_tree(const View* host,
                const FlexStyle& host_style,
                const std::unordered_map<const View*, FlexStyle>& child_styles,
                YogaTree* tree,
                bool set_host_size) {
  tree->root = YGNodeNew();
  apply_flex_style(tree->root, host_style, /*is_host=*/true);
  if (set_host_size) {
    YGNodeStyleSetWidth(tree->root,
                        static_cast<float>(host->bounds().width));
    YGNodeStyleSetHeight(tree->root,
                         static_cast<float>(host->bounds().height));
  }

  for (size_t i = 0; i < host->child_count(); ++i) {
    View* child = host->child_at(i);
    if (!child->is_locally_visible()) {
      continue;
    }
    YGNodeRef yn = YGNodeNew();
    YGNodeSetContext(yn, child);
    const FlexStyle* cs = find_child_style(child_styles, child);
    FlexStyle style = cs ? *cs : FlexStyle{};
    apply_flex_style(yn, style, /*is_host=*/false);
    const bool has_fixed_w = style.width.has_value();
    const bool has_fixed_h = style.height.has_value();
    // Flex-grow children are sized by the flex algorithm. Attaching a measure
    // func fights grow (Yoga treats measured size as a hard intrinsic) and
    // stacks siblings in UiDesigner main_app hbox/vbox previews.
    // flex-grow:0 is a pin (MenuBar / tool_bar) — still need measure so
    // preferred ink height/width apply; treating "has_value" as grow skipped
    // measure and collapsed File/Edit/View/Layer (ui.shell #3).
    const bool has_grow =
        (style.flex_grow.has_value() && *style.flex_grow > 0.f) ||
        (style.flex.has_value() && *style.flex > 0.f);
    if ((!has_fixed_w || !has_fixed_h) && !has_grow) {
      YGNodeSetMeasureFunc(yn, measure_view);
    }
    YGNodeInsertChild(tree->root, yn, YGNodeGetChildCount(tree->root));
    tree->children.push_back(yn);
  }
}

}  // namespace

YogaLayoutManager::YogaLayoutManager() = default;
YogaLayoutManager::~YogaLayoutManager() = default;

void YogaLayoutManager::set_host_style(const FlexStyle& style) {
  host_style_ = style;
}

void YogaLayoutManager::set_child_style(const View* child,
                                        const FlexStyle& style) {
  if (child) {
    child_styles_[child] = style;
  }
}

void YogaLayoutManager::clear_child_style(const View* child) {
  child_styles_.erase(child);
}

void YogaLayoutManager::layout(View* host) {
  layout_static(host);
}

Size YogaLayoutManager::get_preferred_size(const View* host) const {
  return preferred_static(host);
}

void YogaLayoutManager::layout_impl(View* host) {
  if (!host) {
    return;
  }
  YogaTree tree;
  build_tree(host, host_style_, child_styles_, &tree, /*set_host_size=*/true);
  YGNodeCalculateLayout(tree.root, static_cast<float>(host->bounds().width),
                        static_cast<float>(host->bounds().height),
                        YGDirectionLTR);
  for (YGNodeRef yn : tree.children) {
    auto* child = static_cast<View*>(YGNodeGetContext(yn));
    if (!child) {
      continue;
    }
    child->set_bounds({host->bounds().x + round_px(YGNodeLayoutGetLeft(yn)),
                       host->bounds().y + round_px(YGNodeLayoutGetTop(yn)),
                       round_px(YGNodeLayoutGetWidth(yn)),
                       round_px(YGNodeLayoutGetHeight(yn))});
  }
}

Size YogaLayoutManager::preferred_impl(const View* host) const {
  if (!host) {
    return {};
  }
  YogaTree tree;
  build_tree(host, host_style_, child_styles_, &tree, /*set_host_size=*/false);
  YGNodeCalculateLayout(tree.root, YGUndefined, YGUndefined, YGDirectionLTR);
  return {round_px(YGNodeLayoutGetWidth(tree.root)),
          round_px(YGNodeLayoutGetHeight(tree.root))};
}

}  // namespace views
}  // namespace ui
