// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/shell/ambox_view.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/public/plugin_host.h"
#include "tool/command/command.h"
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
    // Shell menus own view navigation; the toolbox does not list it.
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

// Text tool chip with optional accent selected plate (no icon set required).
class AmboxView::ToolButton : public Button {
 public:
  ToolButton(std::string label, std::string id, AmboxView* owner)
      : Button(std::move(label)), id_(std::move(id)), owner_(owner) {}

  const std::string& command_id() const { return id_; }

 protected:
  void paint_self(ui::gfx::Canvas* canvas) override {
    if (!canvas) {
      return;
    }
    const bool on = owner_ && !id_.empty() && owner_->active_command() == id_;
    if (!on) {
      Button::paint_self(canvas);
      return;
    }
    const Theme& t = Theme::current();
    const Rect& b = bounds();
    canvas->fill_rect(b.x, b.y, b.width, b.height, t.accent);
    canvas->stroke_rect(b.x, b.y, b.width, b.height,
                        ui::gfx::color_rgb(70, 160, 230), 1);
    if (!text().empty()) {
      const float scale =
          widget() ? widget()->device_scale_factor() : 1.f;
      const int pad_x = dip_to_px(8, scale);
      const Size ink = measure_text_utf8(text(), scale);
      int text_y = b.y + (b.height - ink.height) / 2;
      if (text_y < b.y) {
        text_y = b.y;
      }
      canvas->draw_text(b.x + pad_x, text_y, utf8_to_wide(text()).c_str(),
                        ui::gfx::color_rgb(255, 255, 255));
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
    header_ = header.get();
    add_child(std::move(header));

    buttons_.reserve(group.items.size());
    for (const auto& item : group.items) {
      auto button =
          std::make_unique<ToolButton>(item.label, item.id, owner);
      button->set_preferred_size({160, 28});
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
  Button* button_at(size_t i) {
    return i < buttons_.size() ? buttons_[i] : nullptr;
  }

 private:
  Label* header_ = nullptr;
  std::vector<Button*> buttons_;
};

AmboxView::AmboxView() {
  MarkupRoot loaded = load_markup("shell/ambox_view.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({180, 240});
    return;
  }
  scroll_ = loaded.ids.find_as<ScrollView>("scroll");
  content_ = loaded.ids.find("content");

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({180, 240});
  add_child(std::move(loaded.root));
  set_preferred_size({180, 240});
  populate_from_plugin_host(nullptr);
}

AmboxView::~AmboxView() = default;

void AmboxView::set_orientation(Orientation orientation) {
  if (orientation_ == orientation) {
    return;
  }
  orientation_ = orientation;
  set_preferred_size(orientation_ == Orientation::kHorizontal ? Size{0, 40}
                                                              : Size{180, 240});
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
}

void AmboxView::fire_command(const std::string& id) {
  set_active_command(id);
  if (handler_) {
    handler_(id);
  }
}

int AmboxView::measure_content_height(float scale) const {
  if (orientation_ == Orientation::kHorizontal) {
    return dip_to_px(36, scale);
  }
  const int pad = dip_to_px(8, scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  const int gap = dip_to_px(6, scale);
  int h = pad;
  for (const auto& block : blocks_) {
    if (!block) {
      continue;
    }
    h += header_h + (btn_h + gap) * static_cast<int>(block->button_count());
    h += pad;
  }
  return h;
}

int AmboxView::measure_content_width(float scale) const {
  const int pad = dip_to_px(8, scale);
  const int gap = dip_to_px(8, scale);
  const int group_gap = dip_to_px(14, scale);
  const int btn_w = dip_to_px(80, scale);
  int w = pad;
  for (const auto& block : blocks_) {
    if (!block) {
      continue;
    }
    // Horizontal mode omits group header width (see layout_horizontal).
    w += (btn_w + gap) * static_cast<int>(block->button_count());
    w += group_gap;
  }
  return w + pad;
}

void AmboxView::layout_vertical(float scale) {
  const Rect& b = bounds();
  const int pad = dip_to_px(8, scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  // Keep a visible gap between stacked tool buttons (visual review P2).
  const int gap = dip_to_px(6, scale);

  if (content_) {
    content_->set_preferred_size({b.width, measure_content_height(scale)});
  }
  if (scroll_) {
    scroll_->layout();
  }

  // Place groups in scroll content space so tall toolboxes clip + scroll
  // instead of overflowing the splitter pane (visual misalignment).
  const Rect area = content_ ? content_->bounds() : b;
  int y = area.y + pad;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    block->set_visible(true);
    const int h =
        header_h + (btn_h + gap) * static_cast<int>(block->button_count());
    const int inner_w = area.width > pad * 2 ? area.width - pad * 2 : 0;
    block->set_bounds({area.x + pad, y, inner_w, h});
    if (View* header = block->header()) {
      header->set_visible(true);
      header->set_bounds({area.x + pad, y, inner_w, header_h});
    }
    int row_y = y + header_h;
    const int btn_w = area.width > pad * 4 ? area.width - pad * 4 : 0;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->set_visible(true);
        button->set_bounds({area.x + pad * 2, row_y, btn_w, btn_h});
      }
      row_y += btn_h + gap;
    }
    y += h + pad;
  }
}

void AmboxView::layout_horizontal(float scale) {
  const Rect& b = bounds();
  const int pad = dip_to_px(8, scale);
  // Wider chip gap so text tools read as separate controls, not one slab.
  const int gap = dip_to_px(8, scale);
  const int group_gap = dip_to_px(14, scale);
  const int btn_w = dip_to_px(80, scale);
  const int btn_h = dip_to_px(28, scale);
  const int row_h = dip_to_px(36, scale);

  if (content_) {
    content_->set_preferred_size(
        {measure_content_width(scale), measure_content_height(scale)});
  }
  if (scroll_) {
    scroll_->layout();
  }

  const Rect area = content_ ? content_->bounds() : b;
  const int y = area.y + (area.height > row_h ? (area.height - row_h) / 2 : 0);
  int x = area.x + pad;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    // Horizontal tool bar: hide group captions ("Select"/"Edit") — button
    // labels already carry the action name (avoids Select+Select clutter).
    if (View* header = block->header()) {
      header->set_visible(false);
    }
    const int block_w =
        (btn_w + gap) * static_cast<int>(block->button_count());
    block->set_visible(true);
    block->set_bounds({x, y, block_w, row_h});
    int bx = x;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->set_visible(true);
        button->set_preferred_size({80, 28});
        button->set_bounds({bx, y + (row_h - btn_h) / 2, btn_w, btn_h});
      }
      bx += btn_w + gap;
    }
    x += block_w + group_gap;
  }
}

void AmboxView::layout() {
  // Size the markup scroll shell first, then place dynamic group blocks.
  View::layout();

  const float scale = widget() ? widget()->device_scale_factor() : 1.f;
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
}

}  // namespace views
}  // namespace ui
