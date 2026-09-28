// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/layout/layout.h"

#include <algorithm>
#include <vector>

namespace ui {
namespace views {
namespace {

int clamp_nonneg(int v) {
  return v < 0 ? 0 : v;
}

// Shrink |sizes| in |indices| by |deficit|, proportional to current sizes.
// Returns remaining deficit that could not be absorbed.
int shrink_proportional(std::vector<int>* sizes,
                        const std::vector<size_t>& indices,
                        int deficit) {
  if (!sizes || deficit <= 0 || indices.empty()) {
    return deficit;
  }
  int total = 0;
  for (size_t i : indices) {
    total += (*sizes)[i];
  }
  if (total <= 0) {
    return deficit;
  }
  const int take = std::min(deficit, total);
  int removed = 0;
  for (size_t n = 0; n < indices.size(); ++n) {
    const size_t i = indices[n];
    int cut = 0;
    if (n + 1 == indices.size()) {
      cut = take - removed;
    } else {
      cut = take * (*sizes)[i] / total;
    }
    if (cut < 0) {
      cut = 0;
    }
    if (cut > (*sizes)[i]) {
      cut = (*sizes)[i];
    }
    (*sizes)[i] -= cut;
    removed += cut;
  }
  return deficit - removed;
}

template <Axis A>
int inset_main(int left, int top, int right, int bottom) {
  if constexpr (axis_traits<A>::horizontal) {
    return left + right;
  } else {
    return top + bottom;
  }
}

template <Axis A>
int inset_cross(int left, int top, int right, int bottom) {
  if constexpr (axis_traits<A>::horizontal) {
    return top + bottom;
  } else {
    return left + right;
  }
}

// Caller already rejected a hidden host, so the ancestor walk is not repeated.
bool child_local(const View* child) {
  return child && view_traits<View>::locally_visible(*child);
}

template <view_node Host>
void layout_fill(Host& host) {
  if (!host.is_visible()) {
    return;
  }
  const Rect& b = view_traits<Host>::bounds(host);
  for (size_t i = 0; i < host.child_count(); ++i) {
    View* child = host.child_at(i);
    if (!child_local(child)) {
      continue;
    }
    child->set_bounds(b);
  }
}

template <view_node Host>
Size preferred_fill(const Host& host) {
  if (!host.is_visible()) {
    return host.preferred_size();
  }
  for (size_t i = 0; i < host.child_count(); ++i) {
    const View* child = host.child_at(i);
    if (!child_local(child)) {
      continue;
    }
    return view_traits<View>::preferred(*child);
  }
  return host.preferred_size();
}

template <Axis A>
struct BoxChild {
  View* child = nullptr;
  int flex = 0;
  int pref = 0;
};

template <Axis A, view_node Host>
Size preferred_box(const Host& host, int left, int top, int right, int bottom,
                   int spacing) {
  int main = 0;
  int cross = 0;
  int visible_n = 0;
  if (host.is_visible()) {
    for (size_t i = 0; i < host.child_count(); ++i) {
      const View* child = host.child_at(i);
      if (!child_local(child)) {
        continue;
      }
      const Size pref = view_traits<View>::preferred(*child);
      ++visible_n;
      main += axis_traits<A>::main(pref);
      cross = std::max(cross, axis_traits<A>::cross(pref));
    }
  }
  if (visible_n > 1) {
    main += spacing * (visible_n - 1);
  }
  return axis_traits<A>::make_size(main + inset_main<A>(left, top, right, bottom),
                                   cross + inset_cross<A>(left, top, right, bottom));
}

template <Axis A, view_node Host, flex_lookup Flex>
void layout_box(Host& host, int left, int top, int right, int bottom,
                int spacing, Flex flex_of) {
  if (!host.is_visible()) {
    return;
  }
  const Rect& host_bounds = view_traits<Host>::bounds(host);
  const int inner_x = host_bounds.x + left;
  const int inner_y = host_bounds.y + top;
  const int inner_w = clamp_nonneg(host_bounds.width - left - right);
  const int inner_h = clamp_nonneg(host_bounds.height - top - bottom);
  int host_main = 0;
  int host_cross = 0;
  if constexpr (axis_traits<A>::horizontal) {
    host_main = inner_w;
    host_cross = inner_h;
  } else {
    host_main = inner_h;
    host_cross = inner_w;
  }

  std::vector<BoxChild<A>> items;
  items.reserve(host.child_count());
  int flex_sum = 0;
  int used = 0;
  for (size_t i = 0; i < host.child_count(); ++i) {
    View* child = host.child_at(i);
    if (!child_local(child)) {
      continue;
    }
    BoxChild<A> item;
    item.child = child;
    item.flex = flex_of(child);
    if (item.flex < 0) {
      item.flex = 0;
    }
    const Size pref = view_traits<View>::preferred(*child);
    item.pref = clamp_nonneg(axis_traits<A>::main(pref));
    if (item.flex > 0) {
      flex_sum += item.flex;
    }
    used += item.pref;
    items.push_back(item);
  }
  if (items.size() > 1) {
    used += spacing * static_cast<int>(items.size() - 1);
  }

  std::vector<int> mains(items.size(), 0);
  const int leftover = host_main - used;
  if (leftover >= 0) {
    int distributed = 0;
    size_t last_flex = items.size();
    for (size_t i = 0; i < items.size(); ++i) {
      mains[i] = items[i].pref;
      if (items[i].flex > 0 && flex_sum > 0) {
        last_flex = i;
      }
    }
    if (flex_sum > 0 && leftover > 0) {
      for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].flex <= 0) {
          continue;
        }
        int share = 0;
        if (i == last_flex) {
          share = leftover - distributed;
        } else {
          share = leftover * items[i].flex / flex_sum;
          distributed += share;
        }
        mains[i] += share;
      }
    }
  } else {
    for (size_t i = 0; i < items.size(); ++i) {
      mains[i] = items[i].pref;
    }
    int deficit = -leftover;
    std::vector<size_t> flex_idx;
    std::vector<size_t> fixed_idx;
    flex_idx.reserve(items.size());
    fixed_idx.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].flex > 0) {
        flex_idx.push_back(i);
      } else {
        fixed_idx.push_back(i);
      }
    }
    deficit = shrink_proportional(&mains, flex_idx, deficit);
    deficit = shrink_proportional(&mains, fixed_idx, deficit);
    (void)deficit;
  }

  int cursor = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      cursor += spacing;
    }
    items[i].child->set_bounds(axis_traits<A>::place(
        inner_x, inner_y, cursor, mains[i], host_cross));
    cursor += mains[i];
  }
}

}  // namespace

Size LayoutManager::get_preferred_size(const View* host) const {
  return host ? host->preferred_size() : Size{};
}

void FillLayout::layout(View* host) {
  layout_static(host);
}

Size FillLayout::get_preferred_size(const View* host) const {
  return preferred_static(host);
}

void FillLayout::layout_impl(View* host) {
  if (!host) {
    return;
  }
  layout_fill(*host);
}

Size FillLayout::preferred_impl(const View* host) const {
  if (!host) {
    return {};
  }
  return preferred_fill(*host);
}

BoxLayout::BoxLayout(Orientation orientation) : orientation_(orientation) {}

void BoxLayout::set_flex_for_view(const View* view, int flex) {
  for (auto& entry : flex_) {
    if (entry.first == view) {
      entry.second = flex;
      return;
    }
  }
  flex_.push_back({view, flex});
}

void BoxLayout::set_inside_border(int inset_px) {
  set_inside_border(inset_px, inset_px, inset_px, inset_px);
}

void BoxLayout::set_inside_border(int left, int top, int right, int bottom) {
  inset_left_ = left < 0 ? 0 : left;
  inset_top_ = top < 0 ? 0 : top;
  inset_right_ = right < 0 ? 0 : right;
  inset_bottom_ = bottom < 0 ? 0 : bottom;
}

void BoxLayout::set_between_child_spacing(int spacing_px) {
  between_child_ = spacing_px < 0 ? 0 : spacing_px;
}

int BoxLayout::flex_of(const View* view) const {
  for (const auto& entry : flex_) {
    if (entry.first == view) {
      return entry.second;
    }
  }
  return 0;
}

template <Axis A>
void BoxLayout::layout_along(View* host) {
  layout_box<A>(
      *host, inset_left_, inset_top_, inset_right_, inset_bottom_,
      between_child_, [this](const View* child) { return flex_of(child); });
}

template <Axis A>
Size BoxLayout::preferred_along(const View* host) const {
  return preferred_box<A>(*host, inset_left_, inset_top_, inset_right_,
                          inset_bottom_, between_child_);
}

void BoxLayout::layout(View* host) {
  layout_static(host);
}

Size BoxLayout::get_preferred_size(const View* host) const {
  return preferred_static(host);
}

void BoxLayout::layout_impl(View* host) {
  if (!host) {
    return;
  }
  if (to_axis(orientation_) == Axis::kHorizontal) {
    layout_along<Axis::kHorizontal>(host);
  } else {
    layout_along<Axis::kVertical>(host);
  }
}

Size BoxLayout::preferred_impl(const View* host) const {
  if (!host) {
    return {};
  }
  if (to_axis(orientation_) == Axis::kHorizontal) {
    return preferred_along<Axis::kHorizontal>(host);
  }
  return preferred_along<Axis::kVertical>(host);
}

template void BoxLayout::layout_along<Axis::kHorizontal>(View*);
template void BoxLayout::layout_along<Axis::kVertical>(View*);
template Size BoxLayout::preferred_along<Axis::kHorizontal>(const View*) const;
template Size BoxLayout::preferred_along<Axis::kVertical>(const View*) const;

}  // namespace views
}  // namespace ui
