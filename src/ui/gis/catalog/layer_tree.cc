// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/layer_tree.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"

namespace ui {
namespace views {

namespace {

// DIPs — match shell body font at high DPI; avoid fixed-px clipping.
constexpr int kRowHeightDip = 26;
constexpr int kCheckSizeDip = 16;
constexpr int kCheckPadDip = 8;
constexpr int kCheckHitPadDip = 4;
constexpr int kLabelGapDip = 8;
constexpr int kRailWidthDip = 3;
constexpr int kRowEndPadDip = 8;
constexpr int kHairlineDip = 1;
constexpr int kIndentStepDip = 14;
constexpr int kExpandSlotDip = 14;
constexpr int kIconSlotDip = 14;

bool is_group_like(LayerKind kind, bool has_children) {
  return kind == LayerKind::kGroup || has_children;
}

// Draw a crisp visibility checkbox (outlined when off, filled + check when on).
void paint_visibility_check(ui::gfx::Canvas* canvas,
                            const Rect& box,
                            bool visible,
                            bool muted,
                            const Theme& t) {
  if (!canvas || box.width <= 0 || box.height <= 0) {
    return;
  }
  const int stroke = std::max(1, box.width / 10);
  if (visible) {
    const ui::gfx::Color fill = muted ? t.control_disabled : t.accent;
    canvas->fill_rect(box.x, box.y, box.width, box.height, fill);
    // Soft outer edge so the plate does not read as a neon block.
    canvas->stroke_rect(box.x, box.y, box.width, box.height,
                        muted ? t.text_muted : t.accent, stroke);
    const ui::gfx::Color mark = muted ? t.text_muted : t.text_bright;
    const int inset = std::max(2, box.width / 5);
    const int tip_x = box.x + inset;
    const int tip_y = box.y + box.height / 2;
    const int mid_x = box.x + box.width / 2 - box.width / 12;
    const int mid_y = box.y + box.height - inset - 1;
    const int end_x = box.x + box.width - inset;
    const int end_y = box.y + inset + 1;
    const int mark_w = std::max(1, stroke + 1);
    canvas->draw_line(tip_x, tip_y, mid_x, mid_y, mark, mark_w);
    canvas->draw_line(mid_x, mid_y, end_x, end_y, mark, mark_w);
  } else {
    canvas->fill_rect(box.x, box.y, box.width, box.height, t.control_bg);
    canvas->stroke_rect(box.x, box.y, box.width, box.height,
                        muted ? t.control_disabled : t.text_muted, stroke);
  }
}

// Compact expand chevron: ▶ collapsed / ▼ expanded (line geometry only).
void paint_expand_chevron(ui::gfx::Canvas* canvas,
                          const Rect& box,
                          bool expanded,
                          bool muted,
                          const Theme& t) {
  if (!canvas || box.width <= 0 || box.height <= 0) {
    return;
  }
  const ui::gfx::Color ink = muted ? t.text_muted : t.text;
  const int stroke = std::max(1, box.width / 8);
  const int cx = box.x + box.width / 2;
  const int cy = box.y + box.height / 2;
  const int half = std::max(2, box.width / 4);
  if (expanded) {
    // Downward triangle outline.
    canvas->draw_line(cx - half, cy - half / 2, cx + half, cy - half / 2, ink,
                      stroke);
    canvas->draw_line(cx + half, cy - half / 2, cx, cy + half / 2, ink, stroke);
    canvas->draw_line(cx, cy + half / 2, cx - half, cy - half / 2, ink, stroke);
  } else {
    // Rightward triangle outline.
    canvas->draw_line(cx - half / 2, cy - half, cx - half / 2, cy + half, ink,
                      stroke);
    canvas->draw_line(cx - half / 2, cy + half, cx + half / 2, cy, ink, stroke);
    canvas->draw_line(cx + half / 2, cy, cx - half / 2, cy - half, ink, stroke);
  }
}

// Small type glyph: vector polyline / raster plate / group folder / unknown dot.
void paint_type_icon(ui::gfx::Canvas* canvas,
                     const Rect& box,
                     LayerKind kind,
                     bool group_like,
                     bool muted,
                     const Theme& t) {
  if (!canvas || box.width <= 0 || box.height <= 0) {
    return;
  }
  const ui::gfx::Color ink = muted ? t.text_muted : t.text;
  const int stroke = std::max(1, box.width / 8);
  const int inset = std::max(1, box.width / 5);
  const LayerKind paint_kind = group_like ? LayerKind::kGroup : kind;
  switch (paint_kind) {
    case LayerKind::kGroup: {
      // Folder: top tab + body outline.
      const int tab_h = std::max(2, box.height / 4);
      canvas->stroke_rect(box.x + inset, box.y + inset, box.width / 3, tab_h,
                          ink, stroke);
      canvas->stroke_rect(box.x + inset, box.y + inset + tab_h - 1,
                          box.width - 2 * inset,
                          box.height - 2 * inset - tab_h + 1, ink, stroke);
      break;
    }
    case LayerKind::kVector: {
      // Polyline zigzag.
      const int x0 = box.x + inset;
      const int x1 = box.x + box.width / 2;
      const int x2 = box.x + box.width - inset;
      const int y0 = box.y + box.height - inset;
      const int y1 = box.y + inset;
      const int y2 = box.y + box.height / 2;
      canvas->draw_line(x0, y0, x1, y1, ink, stroke);
      canvas->draw_line(x1, y1, x2, y2, ink, stroke);
      break;
    }
    case LayerKind::kRaster: {
      // Framed plate with a mid crosshair (grid hint).
      canvas->stroke_rect(box.x + inset, box.y + inset,
                          box.width - 2 * inset, box.height - 2 * inset, ink,
                          stroke);
      const int mx = box.x + box.width / 2;
      const int my = box.y + box.height / 2;
      canvas->draw_line(mx, box.y + inset + 1, mx,
                        box.y + box.height - inset - 1, ink, stroke);
      canvas->draw_line(box.x + inset + 1, my, box.x + box.width - inset - 1, my,
                        ink, stroke);
      break;
    }
    case LayerKind::kUnknown:
    default: {
      // Small centered square as a neutral placeholder.
      const int s = std::max(3, box.width / 3);
      canvas->stroke_rect(box.x + (box.width - s) / 2,
                          box.y + (box.height - s) / 2, s, s, ink, stroke);
      break;
    }
  }
}

}  // namespace

// One hittable layer row: expand / checkbox / type glyph / name.
class LayerTree::LayerRow : public View {
 public:
  LayerRow(std::string id,
           std::string name,
           bool visible,
           LayerKind kind,
           int depth,
           bool expandable,
           bool expanded)
      : id_(std::move(id)),
        name_(std::move(name)),
        visible_(visible),
        kind_(kind),
        depth_(depth),
        expandable_(expandable),
        expanded_(expanded) {
    set_preferred_size({200, kRowHeightDip});
    set_focusable(true);
  }

  void reset(std::string id,
             std::string name,
             bool visible,
             LayerKind kind,
             int depth,
             bool expandable,
             bool expanded) {
    id_ = std::move(id);
    name_ = std::move(name);
    visible_ = visible;
    kind_ = kind;
    depth_ = depth;
    expandable_ = expandable;
    expanded_ = expanded;
    schedule_paint();
  }

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  bool is_layer_visible() const { return visible_; }
  LayerKind kind() const { return kind_; }
  int depth() const { return depth_; }
  bool expandable() const { return expandable_; }
  bool expanded() const { return expanded_; }
  bool group_like() const { return is_group_like(kind_, expandable_); }

  void set_layer_visible(bool visible) {
    if (visible_ == visible) {
      return;
    }
    visible_ = visible;
    schedule_paint();
  }

  void toggle_visible() { set_layer_visible(!visible_); }

  bool hit_expand(int x, int y) const {
    if (!owner_ || !expandable_) {
      return false;
    }
    return owner_->expand_hit(bounds(), depth_).contains(x, y);
  }

  bool hit_checkbox(int x, int y) const {
    if (!owner_) {
      return false;
    }
    return owner_->checkbox_hit(bounds(), depth_).contains(x, y);
  }

  void set_owner(LayerTree* owner) { owner_ = owner; }

  void set_selected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    schedule_paint();
  }

  bool on_mouse_event(const MouseEvent& event) override {
    return owner_ ? owner_->handle_row_mouse(this, event) : false;
  }

  bool on_key_event(const KeyEvent& event) override {
    return owner_ ? owner_->handle_row_key(this, event) : false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !owner_) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const float scale = owner_->scale_factor();
    const int pad = owner_->check_pad();
    const int end_pad = dip_to_px(kRowEndPadDip, scale);
    const int hair = dip_to_px(kHairlineDip, scale);
    const bool muted = !visible_ || !owner_->is_enabled();
    const bool group = group_like();

    // Idle rows stay on panel_bg (parent plate). Only hover / press / select
    // paint a soft overlay — ArcGIS / QGIS density without stacked gray bars.
    if (selected_) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_hover);
      const int rail = owner_->rail_width();
      canvas->fill_rect(b.x, b.y, rail, b.height, t.accent);
    } else if (is_pressed()) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.control_press);
    } else if (is_hovered()) {
      canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_header);
    }

    if (hair > 0 && b.width > pad + end_pad) {
      canvas->fill_rect(b.x + pad, b.bottom() - hair,
                        b.width - pad - end_pad, hair, t.panel_header);
    }

    if (expandable_) {
      paint_expand_chevron(canvas, owner_->expand_ink(b, depth_), expanded_,
                           muted, t);
    }

    const Rect check = owner_->checkbox_ink(b, depth_);
    paint_visibility_check(canvas, check, visible_, muted, t);

    paint_type_icon(canvas, owner_->type_icon_ink(b, depth_), kind_, group,
                    muted, t);

    if (is_focused()) {
      // Row-level focus ring (not checkbox-only) for keyboard / assistive use.
      const int inset = std::max(1, dip_to_px(1, scale));
      draw_focus_ring(canvas, {b.x + inset, b.y + inset,
                               std::max(0, b.width - 2 * inset),
                               std::max(0, b.height - 2 * inset)});
    }

    const std::wstring w = utf8_to_wide(name_);
    const Rect icon = owner_->type_icon_ink(b, depth_);
    const int text_x = icon.right() + owner_->label_gap();
    const Size ink = measure_text_utf8(name_, scale);
    const int text_y = b.y + std::max(0, (b.height - ink.height) / 2);
    // Groups read one step brighter so hierarchy is obvious without bold fonts.
    ui::gfx::Color label = muted ? t.text_muted : t.text;
    if (!muted && (group || selected_)) {
      label = t.text_bright;
    }
    const int clip_w = std::max(0, b.right() - end_pad - text_x);
    if (clip_w > 0) {
      canvas->save();
      canvas->clip_rect(text_x, b.y, clip_w, b.height);
      canvas->draw_text(text_x, text_y, w.c_str(), label);
      canvas->restore();
    }
  }

 private:
  LayerTree* owner_ = nullptr;
  std::string id_;
  std::string name_;
  bool visible_ = true;
  LayerKind kind_ = LayerKind::kUnknown;
  int depth_ = 0;
  bool expandable_ = false;
  bool expanded_ = true;
  bool selected_ = false;
};

LayerTree::LayerTree() {
  set_preferred_size({220, 200});
  // Tab can land on the TOC; arrows then move among rows.
  set_focusable(true);
}

LayerTree::~LayerTree() = default;

float LayerTree::scale_factor() const {
  if (widget()) {
    return widget()->device_scale_factor();
  }
  return 1.f;
}

int LayerTree::row_height() const {
  return dip_to_px(kRowHeightDip, scale_factor());
}

int LayerTree::check_size() const {
  return dip_to_px(kCheckSizeDip, scale_factor());
}

int LayerTree::check_pad() const {
  return dip_to_px(kCheckPadDip, scale_factor());
}

int LayerTree::label_gap() const {
  return dip_to_px(kLabelGapDip, scale_factor());
}

int LayerTree::rail_width() const {
  return std::max(2, dip_to_px(kRailWidthDip, scale_factor()));
}

int LayerTree::indent_step() const {
  return dip_to_px(kIndentStepDip, scale_factor());
}

int LayerTree::expand_slot() const {
  return dip_to_px(kExpandSlotDip, scale_factor());
}

int LayerTree::icon_slot() const {
  return dip_to_px(kIconSlotDip, scale_factor());
}

Rect LayerTree::expand_ink(const Rect& row_bounds, int depth) const {
  const int slot = expand_slot();
  const int size = std::max(8, slot - 2);
  const int x = row_bounds.x + check_pad() + depth * indent_step() +
                (slot - size) / 2;
  const int y = row_bounds.y + (row_bounds.height - size) / 2;
  return {x, y, size, size};
}

Rect LayerTree::expand_hit(const Rect& row_bounds, int depth) const {
  Rect ink = expand_ink(row_bounds, depth);
  const int grow = dip_to_px(kCheckHitPadDip, scale_factor());
  ink.x -= grow;
  ink.y -= grow;
  ink.width += 2 * grow;
  ink.height += 2 * grow;
  if (ink.x < row_bounds.x) {
    ink.width -= (row_bounds.x - ink.x);
    ink.x = row_bounds.x;
  }
  if (ink.y < row_bounds.y) {
    ink.height -= (row_bounds.y - ink.y);
    ink.y = row_bounds.y;
  }
  if (ink.bottom() > row_bounds.bottom()) {
    ink.height = row_bounds.bottom() - ink.y;
  }
  return ink;
}

Rect LayerTree::checkbox_ink(const Rect& row_bounds, int depth) const {
  const int check = check_size();
  const int x =
      row_bounds.x + check_pad() + depth * indent_step() + expand_slot();
  const int cy = row_bounds.y + (row_bounds.height - check) / 2;
  return {x, cy, check, check};
}

Rect LayerTree::checkbox_hit(const Rect& row_bounds, int depth) const {
  Rect ink = checkbox_ink(row_bounds, depth);
  const int grow = dip_to_px(kCheckHitPadDip, scale_factor());
  ink.x -= grow;
  ink.y -= grow;
  ink.width += 2 * grow;
  ink.height += 2 * grow;
  // Keep the hit box inside the row so neighboring rows stay selectable.
  if (ink.x < row_bounds.x) {
    ink.width -= (row_bounds.x - ink.x);
    ink.x = row_bounds.x;
  }
  if (ink.y < row_bounds.y) {
    ink.height -= (row_bounds.y - ink.y);
    ink.y = row_bounds.y;
  }
  if (ink.bottom() > row_bounds.bottom()) {
    ink.height = row_bounds.bottom() - ink.y;
  }
  if (ink.right() > row_bounds.right()) {
    ink.width = row_bounds.right() - ink.x;
  }
  return ink;
}

Rect LayerTree::type_icon_ink(const Rect& row_bounds, int depth) const {
  const int slot = icon_slot();
  const int size = std::max(8, slot - 2);
  const Rect check = checkbox_ink(row_bounds, depth);
  const int x = check.right() + label_gap() / 2 + (slot - size) / 2;
  const int y = row_bounds.y + (row_bounds.height - size) / 2;
  return {x, y, size, size};
}

void LayerTree::on_device_scale_factor_changed(float old_scale, float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  set_preferred_size({dip_to_px(220, s), dip_to_px(200, s)});
  layout();
  schedule_paint();
}

void LayerTree::clear_rows() {
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* child = child_at(i)) {
      child->set_visible(false);
    }
  }
  rows_.clear();
  selected_id_.clear();
  schedule_paint();
}

void LayerTree::clear() {
  model_.clear();
  clear_rows();
}

LayerTree::LayerRow* LayerTree::acquire_row(std::string id,
                                            std::string name,
                                            bool visible,
                                            LayerKind kind,
                                            int depth,
                                            bool expandable,
                                            bool expanded) {
  LayerRow* row = nullptr;
  for (size_t i = 0; i < child_count(); ++i) {
    auto* existing = static_cast<LayerRow*>(child_at(i));
    if (!existing) {
      continue;
    }
    // is_visible() is ancestor-aware: a hidden Catalog tab would make every
    // row look free and overwrite the first layer. Reuse only unused rows.
    bool in_use = false;
    for (LayerRow* used : rows_) {
      if (used == existing) {
        in_use = true;
        break;
      }
    }
    if (!in_use) {
      existing->reset(std::move(id), std::move(name), visible, kind, depth,
                      expandable, expanded);
      existing->set_visible(true);
      existing->set_selected(false);
      row = existing;
      break;
    }
  }
  if (!row) {
    auto created = std::make_unique<LayerRow>(std::move(id), std::move(name),
                                              visible, kind, depth, expandable,
                                              expanded);
    row = created.get();
    add_child(std::move(created));
  }
  row->set_owner(this);
  rows_.push_back(row);
  return row;
}

void LayerTree::append_visible(const LayerDesc& node, int depth) {
  if (node.id.empty() && node.name.empty() && node.children.empty()) {
    return;
  }
  const std::string id = node.id.empty() ? node.name : node.id;
  const std::string name = node.name.empty() ? id : node.name;
  const bool expandable = is_group_like(node.kind, !node.children.empty());
  acquire_row(id, name, node.visible, node.kind, depth, expandable,
              node.expanded);
  if (expandable && node.expanded) {
    for (const LayerDesc& child : node.children) {
      append_visible(child, depth + 1);
    }
  }
}

void LayerTree::rebuild_from_model() {
  // Keep selection id across rebuild; clear_rows wipes it.
  const std::string keep_selected = selected_id_;
  for (size_t i = 0; i < child_count(); ++i) {
    if (View* child = child_at(i)) {
      child->set_visible(false);
    }
  }
  rows_.clear();
  for (const LayerDesc& node : model_) {
    append_visible(node, 0);
  }
  if (!keep_selected.empty()) {
    selected_id_ = keep_selected;
    for (LayerRow* row : rows_) {
      if (row) {
        row->set_selected(row->id() == selected_id_);
      }
    }
  }
  if (!batch_layout_) {
    layout();
    schedule_paint();
  }
}

void LayerTree::add_layer(std::string id, std::string name, bool visible) {
  LayerDesc node;
  node.id = std::move(id);
  node.name = std::move(name);
  node.visible = visible;
  model_.push_back(std::move(node));
  if (batch_layout_) {
    return;
  }
  rebuild_from_model();
}

void LayerTree::set_layers(const std::vector<LayerDesc>& layers) {
  batch_layout_ = true;
  model_ = layers;
  rebuild_from_model();
  std::string active_id;
  // Prefer the first active node in depth-first model order (even if collapsed).
  std::function<void(const LayerDesc&)> find_active =
      [&](const LayerDesc& node) {
        if (!active_id.empty()) {
          return;
        }
        if (node.active) {
          const std::string id = node.id.empty() ? node.name : node.id;
          if (!id.empty()) {
            active_id = id;
            return;
          }
        }
        for (const LayerDesc& child : node.children) {
          find_active(child);
        }
      };
  for (const LayerDesc& node : model_) {
    find_active(node);
  }
  batch_layout_ = false;
  // Defer row placement while the TOC page is still 0-tall (TabStrip body not
  // measured yet). Eager layout on zero height marks every row invisible and
  // View::layout skips invisible children — TOC stays blank after grow.
  if (bounds().height >= row_height()) {
    layout();
  } else {
    mark_needs_layout();
  }
  schedule_paint();
  if (active_id.empty() && !rows_.empty() && rows_.front()) {
    active_id = rows_.front()->id();
  }
  // Silent select: clear_rows already wiped selected_id_, so select_id() would
  // always fire selection_changed_. After OGR replace that cascades into
  // CatalogCall + fill_attribute_rows over every MapLayer feature (AV / hang
  // under showcase SKIP_AMBOX_CATALOG). Host callers sync inspectors
  // explicitly when they need it (on_open / deferred China seed).
  if (!active_id.empty()) {
    selected_id_ = active_id;
    for (LayerRow* row : rows_) {
      if (row) {
        row->set_selected(row->id() == selected_id_);
      }
    }
  }
}

void LayerTree::select_layer(const std::string& id) {
  select_id(id);
}

LayerTree::LayerDesc* LayerTree::find_desc(std::vector<LayerDesc>* nodes,
                                           const std::string& id) {
  if (!nodes) {
    return nullptr;
  }
  for (LayerDesc& node : *nodes) {
    const std::string node_id = node.id.empty() ? node.name : node.id;
    if (node_id == id) {
      return &node;
    }
    if (LayerDesc* child = find_desc(&node.children, id)) {
      return child;
    }
  }
  return nullptr;
}

bool LayerTree::set_desc_visible(std::vector<LayerDesc>* nodes,
                                 const std::string& id,
                                 bool visible) {
  if (LayerDesc* node = find_desc(nodes, id)) {
    node->visible = visible;
    return true;
  }
  return false;
}

bool LayerTree::toggle_desc_expanded(const std::string& id) {
  LayerDesc* node = find_desc(&model_, id);
  if (!node || !is_group_like(node->kind, !node->children.empty())) {
    return false;
  }
  node->expanded = !node->expanded;
  rebuild_from_model();
  return true;
}

bool LayerTree::remove_desc(std::vector<LayerDesc>* nodes,
                            const std::string& id) {
  if (!nodes) {
    return false;
  }
  for (size_t i = 0; i < nodes->size(); ++i) {
    LayerDesc& node = (*nodes)[i];
    const std::string node_id = node.id.empty() ? node.name : node.id;
    if (node_id == id) {
      nodes->erase(nodes->begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
    if (remove_desc(&node.children, id)) {
      return true;
    }
  }
  return false;
}

std::vector<LayerTree::LayerDesc>* LayerTree::sibling_list(
    std::vector<LayerDesc>* nodes,
    const std::string& id) {
  if (!nodes) {
    return nullptr;
  }
  for (size_t i = 0; i < nodes->size(); ++i) {
    LayerDesc& node = (*nodes)[i];
    const std::string node_id = node.id.empty() ? node.name : node.id;
    if (node_id == id) {
      return nodes;
    }
    if (std::vector<LayerDesc>* found = sibling_list(&node.children, id)) {
      return found;
    }
  }
  return nullptr;
}

void LayerTree::set_layer_visible(const std::string& id, bool visible) {
  set_desc_visible(&model_, id, visible);
  if (LayerRow* row = row_at(id)) {
    row->set_layer_visible(visible);
  }
}

bool LayerTree::remove_layer(const std::string& id) {
  if (!remove_desc(&model_, id)) {
    return false;
  }
  const bool was_selected = (selected_id_ == id);
  rebuild_from_model();
  if (was_selected) {
    selected_id_.clear();
    if (!rows_.empty() && rows_.front()) {
      select_id(rows_.front()->id());
    }
  }
  return true;
}

bool LayerTree::move_layer(const std::string& id, int delta) {
  if (delta == 0) {
    return false;
  }
  std::vector<LayerDesc>* siblings = sibling_list(&model_, id);
  if (!siblings || siblings->size() < 2) {
    return false;
  }
  int index = -1;
  for (size_t i = 0; i < siblings->size(); ++i) {
    const LayerDesc& node = (*siblings)[i];
    const std::string node_id = node.id.empty() ? node.name : node.id;
    if (node_id == id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return false;
  }
  const int target = index + delta;
  if (target < 0 || target >= static_cast<int>(siblings->size())) {
    return false;
  }
  std::swap((*siblings)[static_cast<size_t>(index)],
            (*siblings)[static_cast<size_t>(target)]);
  rebuild_from_model();
  return true;
}

void LayerTree::set_visible_changed(VisibleChanged fn) {
  visible_changed_ = std::move(fn);
}

void LayerTree::set_selection_changed(SelectionChanged fn) {
  selection_changed_ = std::move(fn);
}

void LayerTree::set_context_requested(ContextRequested fn) {
  context_requested_ = std::move(fn);
}

bool LayerTree::layer_at(size_t index,
                         std::string* id,
                         std::string* name,
                         bool* visible) const {
  if (index >= rows_.size() || !rows_[index]) {
    return false;
  }
  const LayerRow* row = rows_[index];
  if (id) {
    *id = row->id();
  }
  if (name) {
    *name = row->name();
  }
  if (visible) {
    *visible = row->is_layer_visible();
  }
  return true;
}

bool LayerTree::is_layer_visible(const std::string& id) const {
  if (const LayerRow* row = row_at(id)) {
    return row->is_layer_visible();
  }
  return false;
}

LayerTree::LayerRow* LayerTree::row_at(const std::string& id) const {
  for (LayerRow* row : rows_) {
    if (row && row->id() == id) {
      return row;
    }
  }
  return nullptr;
}

LayerTree::LayerRow* LayerTree::row_at_point(int x, int y) const {
  for (LayerRow* row : rows_) {
    if (row && row->is_visible() && row->bounds().contains(x, y)) {
      return row;
    }
  }
  return nullptr;
}

void LayerTree::select_id(const std::string& id) {
  if (selected_id_ == id) {
    return;
  }
  selected_id_ = id;
  for (LayerRow* row : rows_) {
    if (row) {
      row->set_selected(row->id() == selected_id_);
    }
  }
  if (selection_changed_) {
    selection_changed_(selected_id_);
  }
}

void LayerTree::notify_visible(const std::string& id, bool visible) {
  if (visible_changed_) {
    visible_changed_(id, visible);
  }
}

Point LayerTree::to_screen(int x, int y) const {
  POINT p{x, y};
  HWND hwnd = widget() ? widget()->hwnd() : nullptr;
  if (hwnd) {
    ClientToScreen(hwnd, &p);
  }
  return {p.x, p.y};
}

void LayerTree::layout() {
  ui::gfx::note_layout();
  LayoutScope scope(this);
  const Rect& b = bounds();
  const int row_h = row_height();
  // Early sync (wire_catalog / SeedDocument) often runs before the Catalog
  // TabStrip body has a non-zero height. Hiding every row here used to stick:
  // later layouts skipped invisible children and the Layers TOC stayed empty
  // (ui.shell #2) even after china_city was loaded.
  if (b.width <= 0 || b.height <= 0 || row_h <= 0) {
    return;
  }
  int y = b.y;
  for (LayerRow* row : rows_) {
    if (!row) {
      continue;
    }
    // Keep rows inside the pane (layout_check / no paint overflow).
    if (y + row_h > b.bottom()) {
      row->set_visible(false);
      continue;
    }
    row->set_visible(true);
    row->set_preferred_size({b.width, row_h});
    row->set_bounds({b.x, y, b.width, row_h});
    y += row_h;
  }
}

bool LayerTree::handle_row_mouse(LayerRow* row, const MouseEvent& event) {
  if (!is_enabled() || !row) {
    return false;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 2) {
    select_id(row->id());
    if (context_requested_) {
      context_requested_(row->id(), to_screen(event.x, event.y));
    }
    return true;
  }
  if (event.type == MouseEvent::Type::kUp && event.button == 1) {
    // Expand is independent of checkbox / row selection.
    if (row->hit_expand(event.x, event.y)) {
      toggle_desc_expanded(row->id());
      return true;
    }
    if (row->hit_checkbox(event.x, event.y)) {
      row->toggle_visible();
      set_desc_visible(&model_, row->id(), row->is_layer_visible());
      notify_visible(row->id(), row->is_layer_visible());
    }
    select_id(row->id());
    return true;
  }
  return event.type == MouseEvent::Type::kDown &&
         (event.button == 1 || event.button == 2);
}

bool LayerTree::focus_row_delta(LayerRow* from, int delta) {
  if (rows_.empty() || delta == 0) {
    return false;
  }
  int index = 0;
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i] == from) {
      index = static_cast<int>(i);
      break;
    }
  }
  const int n = static_cast<int>(rows_.size());
  int next = index + delta;
  while (next >= 0 && next < n) {
    LayerRow* row = rows_[static_cast<size_t>(next)];
    if (row && row->is_visible() && row->is_enabled()) {
      select_id(row->id());
      return row->request_focus();
    }
    next += delta > 0 ? 1 : -1;
  }
  return false;
}

bool LayerTree::handle_row_key(LayerRow* row, const KeyEvent& event) {
  if (!is_enabled() || !row || event.type != KeyEvent::Type::kDown) {
    return false;
  }
  switch (event.vk) {
    case VK_UP:
      return focus_row_delta(row, -1);
    case VK_DOWN:
      return focus_row_delta(row, 1);
    case VK_LEFT:
      if (row->expandable() && row->expanded()) {
        toggle_desc_expanded(row->id());
        if (LayerRow* refreshed = row_at(row->id())) {
          refreshed->request_focus();
        }
        return true;
      }
      return focus_row_delta(row, -1);
    case VK_RIGHT:
      if (row->expandable() && !row->expanded()) {
        toggle_desc_expanded(row->id());
        if (LayerRow* refreshed = row_at(row->id())) {
          refreshed->request_focus();
        }
        return true;
      }
      return focus_row_delta(row, 1);
    case VK_SPACE: {
      row->toggle_visible();
      set_desc_visible(&model_, row->id(), row->is_layer_visible());
      notify_visible(row->id(), row->is_layer_visible());
      select_id(row->id());
      return true;
    }
    case VK_RETURN:
      select_id(row->id());
      return true;
    default:
      return false;
  }
}

bool LayerTree::on_mouse_event(const MouseEvent& event) {
  if (!is_enabled()) {
    return false;
  }
  if (LayerRow* row = row_at_point(event.x, event.y)) {
    return handle_row_mouse(row, event);
  }
  return View::on_mouse_event(event);
}

bool LayerTree::on_key_event(const KeyEvent& event) {
  if (!is_enabled() || event.type != KeyEvent::Type::kDown) {
    return false;
  }
  // Tree host focus: arrow/enter drops into the selected (or first) row.
  if (event.vk == VK_DOWN || event.vk == VK_UP || event.vk == VK_RETURN ||
      event.vk == VK_SPACE) {
    LayerRow* start = row_at(selected_id_);
    if (!start) {
      for (LayerRow* row : rows_) {
        if (row && row->is_visible()) {
          start = row;
          break;
        }
      }
    }
    if (!start) {
      return false;
    }
    if (event.vk == VK_DOWN || event.vk == VK_UP) {
      start->request_focus();
      return handle_row_key(start, event);
    }
    start->request_focus();
    return handle_row_key(start, event);
  }
  return false;
}

void LayerTree::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  // Soft right edge so the TOC reads as a docked pane next to the map.
  const int hair = std::max(1, dip_to_px(kHairlineDip, scale_factor()));
  canvas->fill_rect(b.right() - hair, b.y, hair, b.height, t.panel_header);
}

}  // namespace views
}  // namespace ui
