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
    target->items.push_back({std::string(id), label_for_id(id)});
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
  loaded.root->set_preferred_size({180, 240});
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
  set_preferred_size(orientation_ == Orientation::kHorizontal
                         ? Size{0, dip_to_px(40, s)}
                         : Size{dip_to_px(180, s), dip_to_px(240, s)});
  rebuild();
  schedule_paint();
}

void AmboxView::set_groups(std::vector<Group> groups) {
  groups_ = std::move(groups);
  rebuild();
  schedule_paint();
}

void AmboxView::set_command_handler(CommandHandler handler) {
  handler_ = std::move(handler);
}

void AmboxView::populate_from_plugin_host(content::PluginHost* host) {
  populate_from_commands(host ? host->commands() : nullptr);
}

void AmboxView::populate_from_commands(tool::CommandCatalog* catalog) {
  std::vector<tool::CommandCatalog*> catalogs;
  if (catalog) {
    catalogs.push_back(catalog);
  }
  populate_from_commands(catalogs);
}

void AmboxView::populate_from_commands(
    const std::vector<tool::CommandCatalog*>& catalogs,
    std::vector<Group> extra_groups) {
  std::vector<Group> groups = groups_from_catalogs(catalogs);
  if (!extra_groups.empty()) {
    for (Group& bucket : groups) {
      if (bucket.name != "Tools") {
        continue;
      }
      std::vector<Item> kept;
      kept.reserve(bucket.items.size());
      for (const Item& item : bucket.items) {
        bool owned = false;
        for (const Group& extra : extra_groups) {
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
    std::vector<Group> kept_groups;
    kept_groups.reserve(groups.size() + extra_groups.size());
    for (Group& group : groups) {
      if (group.name == "Tools" && group.items.empty()) {
        continue;
      }
      kept_groups.push_back(std::move(group));
    }
    for (Group& extra : extra_groups) {
      if (extra.name.empty()) {
        continue;
      }
      kept_groups.push_back(std::move(extra));
    }
    groups = std::move(kept_groups);
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
  if (active_id_ == id) {
    return;
  }
  active_id_ = std::move(id);
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
  set_active_command(id);
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
  set_preferred_size(orientation_ == Orientation::kHorizontal
                         ? Size{0, dip_to_px(40, s)}
                         : Size{dip_to_px(180, s), dip_to_px(240, s)});
  layout();
  schedule_paint();
}

int AmboxView::measure_content_height(float scale) const {
  if (orientation_ == Orientation::kHorizontal) {
    const int host_h = bounds().height;
    if (host_h > 0) {
      return host_h;
    }
    return dip_to_px(36, scale);
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
        btn_w = icon + dip_to_px(10, scale) + ink.width + dip_to_px(8, scale);
        btn_w = std::clamp(btn_w, dip_to_px(48, scale), dip_to_px(120, scale));
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

  if (content_) {
    content_->set_preferred_size({b.width, measure_content_height(scale)});
  }
  if (scroll_) {
    scroll_->layout();
  }

  const Rect area = content_ ? content_->bounds() : b;
  int y = area.y + p;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    block->set_visible(true);
    const int h =
        header_h + (btn_h + gap) * static_cast<int>(block->button_count());
    const int inner_w = area.width > p * 2 ? area.width - p * 2 : 0;
    block->set_bounds({area.x + p, y, inner_w, h});
    if (View* header = block->header()) {
      header->set_visible(true);
      header->set_bounds({area.x + p, y, inner_w, header_h});
    }
    int row_y = y + header_h;
    const int btn_w = area.width > p * 4 ? area.width - p * 4 : 0;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->set_visible(true);
        button->set_bounds({area.x + p * 2, row_y, btn_w, btn_h});
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
  const int host_h = b.height > 0 ? b.height : dip_to_px(40, scale);
  const int pad_y = std::max(dip_to_px(2, scale), host_h / 8);
  const int btn_h =
      std::min(dip_to_px(28, scale), std::max(dip_to_px(14, scale),
                                              host_h - pad_y * 2));
  const int row_h = host_h;
  const bool icon_only = effective_chip_style() == ChipStyle::kIconOnly;
  const int icon = icon_slot(scale);

  if (content_) {
    content_->set_preferred_size(
        {measure_content_width(scale), measure_content_height(scale)});
  }
  if (scroll_) {
    scroll_->layout();
  }

  const Rect area = content_ ? content_->bounds() : b;
  const int y = area.y + (area.height > row_h ? (area.height - row_h) / 2 : 0);
  int x = area.x + p;
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
        btn_w = icon + dip_to_px(10, scale) + ink.width + dip_to_px(8, scale);
        btn_w = std::clamp(btn_w, dip_to_px(48, scale), dip_to_px(120, scale));
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
  View::layout();
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
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);

  const float scale = scale_factor();
  const int hair = std::max(1, dip_to_px(1, scale));
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

}  // namespace views
}  // namespace ui
