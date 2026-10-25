// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/ambox_view.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/plugin_host.h"
#include "tool/command/command.h"
#include "ui/gis/shell/tool_glyphs.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/scroll_view.h"
#include "ui/views/primitives/text/label.h"

namespace ui {
namespace views {
namespace {

// Glyph + label chip. Shared with .tool_bar in main_app.ui.css.
constexpr int kHorizontalChipDip = 48;

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

std::string label_for_id(std::string_view id) {
  const auto slash = id.find_last_of('.');
  if (slash == std::string_view::npos) {
    return std::string(id);
  }
  return std::string(id.substr(slash + 1));
}

// Human labels for packed horizontal chips (group headers are hidden there).
std::string label_for_command(std::string_view id) {
  if (id == "select" || id == "selection") {
    return "Select";
  }
  if (id == "identify") {
    return "Identify";
  }
  if (id == "selection.clear") {
    return "Clear";
  }
  if (id == "selection.point") {
    return "Sel pt";
  }
  if (id == "selection.rect") {
    return "Sel rect";
  }
  if (id == "selection.polygon") {
    return "Sel poly";
  }
  if (id == "edit.append.point") {
    return "Point";
  }
  if (id == "edit.append.linestring") {
    return "Line";
  }
  if (id == "edit.append.polygon") {
    return "Polygon";
  }
  if (id == "edit.undo") {
    return "Undo";
  }
  if (id == "edit.redo") {
    return "Redo";
  }
  if (id == "edit.cancel") {
    return "Cancel";
  }
  if (id == "edit.vertex") {
    return "Vertex";
  }
  if (id == "view3d.fps") {
    return "FPS";
  }
  if (id == "view3d.sphere") {
    return "Sphere";
  }
  if (id == "view3d.full" || id == "view.full") {
    return "Full";
  }
  if (id == "view3d.trackball") {
    return "Orbit";
  }
  return label_for_id(id);
}

void ensure_item(std::vector<AmboxView::Item>* items,
                 const char* id,
                 const char* label) {
  if (!items) {
    return;
  }
  for (const auto& item : *items) {
    if (item.id == id) {
      return;
    }
  }
  items->push_back({id, label});
}

bool has_item_id(const std::vector<AmboxView::Item>& items,
                 std::string_view id) {
  for (const auto& item : items) {
    if (item.id == id) {
      return true;
    }
  }
  return false;
}

void append_catalog_ids(tool::CommandCatalog* catalog,
                        AmboxView::Group* select,
                        AmboxView::Group* edit,
                        AmboxView::Group* tools) {
  if (!catalog || !select || !edit || !tools) {
    return;
  }
  catalog->for_each([&](std::string_view id) {
    if (id.empty() || starts_with(id, "view.")) {
      return;
    }
    AmboxView::Group* target = nullptr;
    if (starts_with(id, "selection.") || id == "select" || id == "identify") {
      target = select;
    } else if (starts_with(id, "edit.")) {
      target = edit;
    } else if (!starts_with(id, "flash.")) {
      target = tools;
    }
    if (!target || has_item_id(target->items, id)) {
      return;
    }
    target->items.push_back({std::string(id), label_for_command(id)});
  });
}

std::vector<AmboxView::Group> groups_from_catalogs(
    const std::vector<tool::CommandCatalog*>& catalogs) {
  AmboxView::Group select{"Select", {}};
  AmboxView::Group edit{"Edit", {}};
  AmboxView::Group tools{"Tools", {}};

  for (tool::CommandCatalog* catalog : catalogs) {
    append_catalog_ids(catalog, &select, &edit, &tools);
  }

  ensure_item(&select.items, "select", "Select");
  ensure_item(&edit.items, "edit.append.point", "Point");
  ensure_item(&edit.items, "edit.append.linestring", "Line");
  ensure_item(&edit.items, "edit.append.polygon", "Polygon");
  ensure_item(&edit.items, "edit.undo", "Undo");
  ensure_item(&edit.items, "edit.cancel", "Cancel");

  std::vector<AmboxView::Group> groups;
  groups.push_back(std::move(select));
  groups.push_back(std::move(edit));
  if (!tools.items.empty()) {
    groups.push_back(std::move(tools));
  }
  return groups;
}

std::vector<AmboxView::Group> merge_catalog_and_extras(
    std::vector<AmboxView::Group> groups,
    std::vector<AmboxView::Group> extra_groups) {
  if (extra_groups.empty()) {
    return groups;
  }
  for (AmboxView::Group& bucket : groups) {
    if (bucket.name != "Tools") {
      continue;
    }
    std::vector<AmboxView::Item> kept;
    kept.reserve(bucket.items.size());
    for (const AmboxView::Item& item : bucket.items) {
      bool owned = false;
      for (const AmboxView::Group& extra : extra_groups) {
        if (has_item_id(extra.items, item.id)) {
          owned = true;
          break;
        }
      }
      if (!owned) {
        kept.push_back(item);
      }
    }
    bucket.items = std::move(kept);
  }
  std::vector<AmboxView::Group> kept_groups;
  kept_groups.reserve(groups.size() + extra_groups.size());
  for (AmboxView::Group& group : groups) {
    if (group.name == "Tools" && group.items.empty()) {
      continue;
    }
    kept_groups.push_back(std::move(group));
  }
  for (AmboxView::Group& extra : extra_groups) {
    if (extra.name.empty()) {
      continue;
    }
    kept_groups.push_back(std::move(extra));
  }
  return kept_groups;
}

}  // namespace

// Icon + optional label chip. Active tool: accent plate + left rail.
class AmboxView::ToolButton : public Button {
 public:
  ToolButton(std::string label, std::string id, AmboxView* owner)
      : Button(std::move(label)), id_(std::move(id)), owner_(owner) {}

  const std::string& command_id() const { return id_; }

  bool on_key_event(const KeyEvent& e) override {
    if (Button::on_key_event(e)) {
      return true;
    }
    if (!owner_ || e.type != KeyEvent::Type::kDown) {
      return false;
    }
    if (e.vk == VK_LEFT || e.vk == VK_RIGHT || e.vk == VK_UP ||
        e.vk == VK_DOWN) {
      return owner_->move_tool_focus(this, e.vk);
    }
    return false;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    const bool on = owner_ && !id_.empty() && owner_->active_command() == id_;
    const float scale = owner_ ? owner_->scale_factor() : 1.f;
    const int rail = std::max(2, dip_to_px(3, scale));
    const int edge = std::max(1, dip_to_px(1, scale));
    const int icon = owner_ ? owner_->icon_slot(scale) : dip_to_px(16, scale);
    const bool show_text =
        owner_ &&
        owner_->effective_chip_style() == AmboxView::ChipStyle::kIconText &&
        !text().empty();

    ui::gfx::Color fill = t.control_fill;
    if (!is_enabled()) {
      fill = t.control_disabled;
    } else if (on) {
      fill = t.accent;
    } else if (is_pressed()) {
      fill = t.control_press;
    } else if (is_hovered()) {
      fill = t.control_hover;
    }
    canvas->fill_rect(b.x, b.y, b.width, b.height, fill);

    if (on && is_enabled()) {
      canvas->fill_rect(b.x, b.y, rail, b.height, t.text_bright);
      canvas->stroke_rect(b.x, b.y, b.width, b.height, t.accent, edge);
    } else if (is_enabled()) {
      canvas->stroke_rect(b.x, b.y, b.width, b.height, t.panel_header, edge);
    }

    const int pad_x = dip_to_px(6, scale);
    const int content_x = b.x + pad_x + (on ? rail : 0);
    const int icon_y = b.y + (b.height - icon) / 2;
    const Rect icon_box{content_x, icon_y, icon, icon};
    const ui::gfx::Color glyph_ink =
        is_enabled() ? t.text_bright : t.text_muted;
    detail::paint_tool_glyph(canvas, icon_box, id_, glyph_ink, !is_enabled());

    if (show_text) {
      const int text_x = icon_box.right() + dip_to_px(4, scale);
      const Size ink = measure_text_utf8(text(), scale);
      int text_y = b.y + (b.height - ink.height) / 2;
      if (text_y < b.y) {
        text_y = b.y;
      }
      canvas->save();
      canvas->clip_rect(text_x, b.y, std::max(0, b.right() - text_x - edge),
                        b.height);
      canvas->draw_text(text_x, text_y, utf8_to_wide(text()).c_str(),
                        glyph_ink);
      canvas->restore();
    }

    if (is_focused()) {
      const int inset = edge;
      draw_focus_ring(canvas, {b.x + inset, b.y + inset,
                               std::max(0, b.width - 2 * inset),
                               std::max(0, b.height - 2 * inset)});
    }
  }

 private:
  std::string id_;
  AmboxView* owner_ = nullptr;
};

class AmboxView::GroupBlock : public View {
 public:
  explicit GroupBlock(const AmboxView::Group& group, AmboxView* owner) {
    auto header = std::make_unique<Label>(group.name);
    header->set_preferred_size({160, 28});
    header->set_color(Theme::current().text_muted);
    header_ = header.get();
    add_child(std::move(header));

    buttons_.reserve(group.items.size());
    for (const auto& item : group.items) {
      auto button =
          std::make_unique<ToolButton>(item.label, item.id, owner);
      button->set_preferred_size({80, 28});
      const std::string id = item.id;
      button->set_click([owner, id]() {
        if (owner) {
          owner->fire_command(id);
        }
      });
      buttons_.push_back(button.get());
      add_child(std::move(button));
    }
    set_preferred_size({180, 28 + 28 * static_cast<int>(buttons_.size())});
  }

  Label* header() { return header_; }
  size_t button_count() const { return buttons_.size(); }
  ToolButton* button_at(size_t i) {
    return i < buttons_.size() ? buttons_[i] : nullptr;
  }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas || !header_ || !header_->is_visible()) {
      return;
    }
    const Theme& t = Theme::current();
    const Rect& h = header_->bounds();
    if (h.width <= 0 || h.height <= 0) {
      return;
    }
    canvas->fill_rect(h.x, h.y, h.width, h.height, t.panel_header);
    const float scale = widget() ? widget()->device_scale_factor() : 1.f;
    const int hair = std::max(1, dip_to_px(1, scale));
    canvas->fill_rect(h.x, h.bottom() - hair, h.width, hair, t.control_fill);
  }

 private:
  Label* header_ = nullptr;
  std::vector<ToolButton*> buttons_;
};

AmboxView::AmboxView() {
  MarkupRoot loaded = load_markup("shell/ambox_view.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({180, 240});
    return;
  }
  scroll_ = loaded.ids.find_as<ScrollView>("scroll");
  content_ = loaded.ids.find("content");
  // Tab can land on the toolbox; arrows then move among chips.
  set_focusable(true);

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  // Markup installs Yoga on the scroll shell when it has a content child.
  // Chip positions are owned by layout_vertical/horizontal — clear Yoga so it
  // cannot overwrite ToolButton bounds with a 0-width measure.
  if (scroll_) {
    scroll_->set_layout_manager(nullptr);
    scroll_->set_preferred_size({0, 0});
  }
  if (content_) {
    content_->set_layout_manager(nullptr);
    content_->set_preferred_size({0, 0});
  }
  loaded.root->set_preferred_size({0, 0});
  add_child(std::move(loaded.root));
  set_preferred_size({180, 240});
  populate_from_plugin_host(nullptr);
}

AmboxView::~AmboxView() = default;

float AmboxView::scale_factor() const {
  return widget() ? widget()->device_scale_factor() : 1.f;
}

int AmboxView::icon_slot(float scale) const {
  return dip_to_px(16, scale);
}

int AmboxView::chip_gap(float scale) const {
  return dip_to_px(orientation_ == Orientation::kHorizontal ? 6 : 6, scale);
}

int AmboxView::group_gap(float scale) const {
  return dip_to_px(12, scale);
}

int AmboxView::pad(float scale) const {
  return dip_to_px(8, scale);
}

AmboxView::ChipStyle AmboxView::effective_chip_style() const {
  return chip_style_;
}

void AmboxView::set_chip_style(ChipStyle style) {
  if (chip_style_ == style) {
    return;
  }
  chip_style_ = style;
  layout();
  schedule_paint();
}

void AmboxView::set_orientation(Orientation orientation) {
  if (orientation_ == orientation) {
    return;
  }
  orientation_ = orientation;
  const float s = scale_factor();
  if (orientation_ == Orientation::kHorizontal) {
    pin_horizontal_shell(s);
  } else {
    set_preferred_size({dip_to_px(180, s), dip_to_px(240, s)});
  }
  rebuild();
  schedule_paint();
}

void AmboxView::set_groups(std::vector<Group> groups) {
  groups_ = std::move(groups);
  rebuild();
  schedule_paint();
}

void AmboxView::replace_groups_begin() {
  pending_groups_.clear();
}

void AmboxView::replace_groups_add(const char* name) {
  Group group;
  group.name = (name && name[0]) ? name : "";
  pending_groups_.push_back(std::move(group));
}

void AmboxView::replace_groups_add_item(const char* id, const char* label) {
  if (pending_groups_.empty() || !id || !id[0]) {
    return;
  }
  Item item;
  item.id = id;
  item.label = (label && label[0]) ? label : item.id;
  pending_groups_.back().items.push_back(std::move(item));
}

void AmboxView::replace_groups_commit() {
  set_groups(std::move(pending_groups_));
  pending_groups_.clear();
}

void AmboxView::set_command_handler(CommandHandler handler) {
  handler_fn_ = nullptr;
  handler_baton_ = nullptr;
  handler_ = std::move(handler);
}

void AmboxView::set_command_handler_fn(CommandHandlerFn fn, void* baton) {
  handler_ = {};
  handler_fn_ = fn;
  handler_baton_ = baton;
}

void AmboxView::populate_from_plugin_host(content::PluginHost* host) {
  populate_from_commands(host ? host->commands() : nullptr);
}

void AmboxView::populate_from_commands(tool::CommandCatalog* catalog) {
  tool::CommandCatalog* one = catalog;
  populate_from_catalogs(catalog ? &one : nullptr, catalog ? 1u : 0u, false);
}

void AmboxView::populate_from_commands(
    const std::vector<tool::CommandCatalog*>& catalogs,
    std::vector<Group> extra_groups) {
  pending_groups_ = std::move(extra_groups);
  populate_from_catalogs(catalogs.empty() ? nullptr : catalogs.data(),
                         catalogs.size(), !pending_groups_.empty());
}

void AmboxView::populate_from_catalogs(tool::CommandCatalog* const* catalogs,
                                       size_t catalog_count,
                                       bool merge_extras) {
  std::vector<tool::CommandCatalog*> list;
  list.reserve(catalog_count);
  for (size_t i = 0; i < catalog_count; ++i) {
    if (catalogs && catalogs[i]) {
      list.push_back(catalogs[i]);
    }
  }
  std::vector<Group> groups = groups_from_catalogs(list);
  if (merge_extras) {
    groups = merge_catalog_and_extras(std::move(groups),
                                      std::move(pending_groups_));
    pending_groups_.clear();
  }
  set_groups(std::move(groups));
}

void AmboxView::rebuild() {
  if (!content_) {
    return;
  }
  content_->remove_all_children();
  blocks_.clear();
  blocks_.reserve(groups_.size());
  for (const auto& group : groups_) {
    auto block = std::make_unique<GroupBlock>(group, this);
    blocks_.push_back(block.get());
    content_->add_child(std::move(block));
  }
  layout();
}

void AmboxView::set_active_command(std::string id) {
  set_active_command_id(id.c_str());
}

void AmboxView::set_active_command_id(const char* id) {
  const std::string next = (id && id[0]) ? id : "";
  if (active_id_ == next) {
    return;
  }
  active_id_ = next;
  schedule_paint();
  for (GroupBlock* block : blocks_) {
    if (!block) {
      continue;
    }
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->schedule_paint();
      }
    }
  }
}

void AmboxView::fire_command(const std::string& id) {
  set_active_command_id(id.c_str());
  if (handler_fn_) {
    handler_fn_(handler_baton_, id.c_str());
    return;
  }
  if (handler_) {
    handler_(id);
  }
}

void AmboxView::collect_tool_buttons(std::vector<ToolButton*>* out) const {
  if (!out) {
    return;
  }
  out->clear();
  for (GroupBlock* block : blocks_) {
    if (!block || !block->is_visible()) {
      continue;
    }
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (ToolButton* button = block->button_at(i)) {
        if (button->is_visible() && button->is_enabled()) {
          out->push_back(button);
        }
      }
    }
  }
}

bool AmboxView::move_tool_focus(ToolButton* from, std::uint32_t vk) {
  std::vector<ToolButton*> tools;
  collect_tool_buttons(&tools);
  if (tools.empty()) {
    return false;
  }
  int index = 0;
  for (size_t i = 0; i < tools.size(); ++i) {
    if (tools[i] == from) {
      index = static_cast<int>(i);
      break;
    }
  }
  const bool horizontal = orientation_ == Orientation::kHorizontal;
  int delta = 0;
  if (vk == VK_RIGHT || (vk == VK_DOWN && !horizontal)) {
    delta = 1;
  } else if (vk == VK_LEFT || (vk == VK_UP && !horizontal)) {
    delta = -1;
  } else if (vk == VK_DOWN && horizontal) {
    delta = 1;
  } else if (vk == VK_UP && horizontal) {
    delta = -1;
  } else {
    return false;
  }
  const int n = static_cast<int>(tools.size());
  int next = (index + delta) % n;
  if (next < 0) {
    next += n;
  }
  return tools[static_cast<size_t>(next)]->request_focus();
}

bool AmboxView::on_key_event(const KeyEvent& event) {
  if (event.type != KeyEvent::Type::kDown) {
    return false;
  }
  // When the toolbox itself is focused, arrow keys enter the first chip.
  if (event.vk == VK_RIGHT || event.vk == VK_DOWN || event.vk == VK_LEFT ||
      event.vk == VK_UP) {
    std::vector<ToolButton*> tools;
    collect_tool_buttons(&tools);
    if (!tools.empty()) {
      return tools.front()->request_focus();
    }
  }
  return false;
}

void AmboxView::on_device_scale_factor_changed(float old_scale,
                                              float new_scale) {
  View::on_device_scale_factor_changed(old_scale, new_scale);
  const float s = new_scale > 0.f ? new_scale : 1.f;
  if (orientation_ == Orientation::kHorizontal) {
    pin_horizontal_shell(s);
  } else {
    set_preferred_size({dip_to_px(180, s), dip_to_px(240, s)});
  }
  layout();
  schedule_paint();
}

int AmboxView::horizontal_row_px(float scale) const {
  return dip_to_px(kHorizontalChipDip, scale);
}

void AmboxView::pin_horizontal_shell(float scale) {
  const int row = horizontal_row_px(scale);
  // FillLayout reports the markup child, whose CSS is the 240 DIP dock.
  // Keep this view's own hint on one chip row, and do not copy bounds height
  // back — that is how a stretched band became permanent.
  set_preferred_size({0, row});
  if (View* shell = child_count() > 0 ? child_at(0) : nullptr) {
    shell->set_preferred_size({0, row});
  }
  if (scroll_) {
    scroll_->set_preferred_size({0, row});
  }
  if (content_) {
    const int width = std::max(content_->preferred_size().width, 0);
    content_->set_preferred_size({width, row});
  }
}

int AmboxView::measure_content_height(float scale) const {
  if (orientation_ == Orientation::kHorizontal) {
    return horizontal_row_px(scale);
  }
  const int p = pad(scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  const int gap = chip_gap(scale);
  int h = p;
  for (const auto& block : blocks_) {
    if (!block) {
      continue;
    }
    h += header_h + (btn_h + gap) * static_cast<int>(block->button_count());
    h += p;
  }
  return h;
}

int AmboxView::measure_content_width(float scale) const {
  const int p = pad(scale);
  const int gap = chip_gap(scale);
  const int ggap = group_gap(scale);
  const bool icon_only = effective_chip_style() == ChipStyle::kIconOnly;
  const int icon = icon_slot(scale);
  int w = p;
  for (const auto& block : blocks_) {
    if (!block) {
      continue;
    }
    for (size_t i = 0; i < block->button_count(); ++i) {
      ToolButton* button = block->button_at(i);
      int btn_w = icon + dip_to_px(12, scale);
      if (!icon_only && button) {
        const Size ink = measure_text_utf8(button->text(), scale);
        btn_w = icon + dip_to_px(10, scale) + ink.width + dip_to_px(10, scale);
        btn_w = std::clamp(btn_w, dip_to_px(56, scale), dip_to_px(140, scale));
      } else {
        btn_w = std::max(dip_to_px(28, scale), icon + dip_to_px(12, scale));
      }
      w += btn_w + gap;
    }
    w += ggap;
  }
  return w + p;
}

void AmboxView::layout_vertical(float scale) {
  const Rect& b = bounds();
  const int p = pad(scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  const int gap = chip_gap(scale);

  // Chip geometry is owned here. Do not trust ScrollView/content bounds — an
  // early 0x0 populate can leave the scroll shell stuck at width 0 while this
  // host already has the inspector page size (Tools buttons never hit-test).
  int host_w = std::max(0, b.width);
  if (host_w <= 0 && parent()) {
    host_w = std::max(0, parent()->bounds().width);
  }
  const int host_h = std::max(0, b.height);
  const int content_h = measure_content_height(scale);
  if (scroll_ && host_w > 0 && host_h > 0) {
    scroll_->set_bounds(b);
  }
  if (content_) {
    content_->set_preferred_size({host_w, content_h});
    // Place content from the host origin. Do not call scroll_->layout() here:
    // ScrollView::apply_content_bounds rewrites content from the scroll box,
    // and a stale 0x0 scroll shell would zero every chip width again.
    content_->set_bounds({b.x, b.y, host_w, content_h});
  }

  const int area_w = host_w;
  const int area_x = b.x;
  const int area_y = b.y;
  int y = area_y + p;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    block->set_visible(true);
    const int h =
        header_h + (btn_h + gap) * static_cast<int>(block->button_count());
    const int inner_w = area_w > p * 2 ? area_w - p * 2 : 0;
    block->set_bounds({area_x + p, y, inner_w, h});
    if (View* header = block->header()) {
      header->set_visible(true);
      header->set_bounds({area_x + p, y, inner_w, header_h});
    }
    int row_y = y + header_h;
    const int btn_w = area_w > p * 4 ? area_w - p * 4 : 0;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->set_visible(true);
        button->set_bounds({area_x + p * 2, row_y, btn_w, btn_h});
      }
      row_y += btn_h + gap;
    }
    y += h + p;
  }
}

void AmboxView::layout_horizontal(float scale) {
  const Rect& b = bounds();
  const int p = pad(scale);
  const int gap = chip_gap(scale);
  const int ggap = group_gap(scale);
  // Never adopt bounds().height. A tall host (shared 240 DIP dock CSS, or a
  // previous pass that wrote the stretched size back) must not stick.
  pin_horizontal_shell(scale);
  const int row_h = horizontal_row_px(scale);
  const int btn_h =
      std::min(dip_to_px(28, scale),
               std::max(dip_to_px(14, scale), row_h - dip_to_px(8, scale)));
  const bool icon_only = effective_chip_style() == ChipStyle::kIconOnly;
  const int icon = icon_slot(scale);
  // Content may be wider than the host; ScrollView scrolls inside the clipped
  // Ambox bounds so chips never resize the map_column into the inspector
  // (ui.shell no_sibling_overlap).
  const int content_w = measure_content_width(scale);

  if (content_) {
    content_->set_preferred_size({content_w, row_h});
  }
  if (scroll_) {
    scroll_->layout();
  }

  const int y = b.y;
  int x = b.x + p;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    if (View* header = block->header()) {
      header->set_visible(false);
    }
    int block_w = 0;
    std::vector<int> widths;
    widths.reserve(block->button_count());
    for (size_t i = 0; i < block->button_count(); ++i) {
      ToolButton* button = block->button_at(i);
      int btn_w = icon + dip_to_px(12, scale);
      if (!icon_only && button) {
        const Size ink = measure_text_utf8(button->text(), scale);
        btn_w = icon + dip_to_px(10, scale) + ink.width + dip_to_px(10, scale);
        btn_w = std::clamp(btn_w, dip_to_px(56, scale), dip_to_px(140, scale));
      } else {
        btn_w = std::max(dip_to_px(28, scale), icon + dip_to_px(12, scale));
      }
      widths.push_back(btn_w);
      block_w += btn_w + gap;
    }
    block->set_visible(true);
    block->set_bounds({x, y, block_w, row_h});
    int bx = x;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        const int btn_w = widths[i];
        button->set_visible(true);
        button->set_preferred_size({btn_w, btn_h});
        button->set_bounds({bx, y + (row_h - btn_h) / 2, btn_w, btn_h});
      }
      bx += widths[i] + gap;
    }
    x += block_w + ggap;
  }
}

void AmboxView::layout() {
  const Rect& b = bounds();
  // Position the scroll shell before View::layout walks children. FillLayout
  // alone left vertical Tools at scroll 0x0 after an early 0-area rebuild.
  if (scroll_ && b.width > 0 && b.height > 0) {
    scroll_->set_bounds(b);
  }
  View::layout();
  if (scroll_ && b.width > 0 && b.height > 0) {
    const Rect& sb = scroll_->bounds();
    if (sb.width != b.width || sb.height != b.height || sb.x != b.x ||
        sb.y != b.y) {
      scroll_->set_bounds(b);
    }
  }
  const float scale = scale_factor();
  if (orientation_ == Orientation::kHorizontal) {
    layout_horizontal(scale);
  } else {
    layout_vertical(scale);
  }
}

void AmboxView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  const float scale = scale_factor();
  // Repair vertical Tools chips that were measured at 0x0 and never reflowed.
  if (orientation_ == Orientation::kVertical && b.width > 0 && b.height > 0 &&
      !blocks_.empty() && blocks_[0] && blocks_[0]->button_count() > 0) {
    View* chip = blocks_[0]->button_at(0);
    if (chip && chip->bounds().width <= 0) {
      layout_vertical(scale);
    }
  }
  if (orientation_ == Orientation::kHorizontal) {
    // Pack chip horizon to content width; leftover rail is not panel_bg
    // (that empty dark strip failed map_nav_toolbar / dark_chrome gates).
    const int packed = std::min(b.width, measure_content_width(scale));
    if (packed > 0) {
      canvas->fill_rect(b.x, b.y, packed, b.height, t.panel_bg);
    }
    if (b.width > packed) {
      canvas->fill_rect(b.x + packed, b.y, b.width - packed, b.height,
                        t.map_placeholder);
    }
  } else {
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  }

  const int hair = std::max(1, dip_to_px(1, scale));
  std::vector<ToolButton*> tools;
  collect_tool_buttons(&tools);
  if (tools.empty() && orientation_ == Orientation::kVertical) {
    // Side Tools tab empty-state (visual_review #10). Bright ink so the shell
    // chrome_readable gate still sees label mass when plugin groups are empty.
    canvas->draw_text(b.x + dip_to_px(12, scale), b.y + dip_to_px(16, scale),
                      L"No tools in this workspace", t.text_bright);
  }
  if (orientation_ == Orientation::kHorizontal) {
    canvas->fill_rect(b.x, b.bottom() - hair, b.width, hair, t.panel_header);
    for (size_t i = 1; i < blocks_.size(); ++i) {
      View* prev = blocks_[i - 1];
      View* next = blocks_[i];
      if (!prev || !next || !prev->is_visible() || !next->is_visible()) {
        continue;
      }
      const Rect& a = prev->bounds();
      const Rect& c = next->bounds();
      if (c.x <= a.right()) {
        continue;
      }
      const int mid = (a.right() + c.x) / 2;
      const int inset = std::max(dip_to_px(2, scale), hair * 4);
      canvas->fill_rect(mid, b.y + inset, hair,
                        std::max(0, b.height - inset * 2), t.panel_header);
    }
  } else {
    canvas->fill_rect(b.right() - hair, b.y, hair, b.height, t.panel_header);
  }
}

std::unique_ptr<AmboxView> make_ambox_view() {
  return std::make_unique<AmboxView>();
}

AmboxView* ambox_view_new() {
  return new AmboxView();
}

}  // namespace views
}  // namespace ui
