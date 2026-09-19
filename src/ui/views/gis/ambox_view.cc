// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/gis/ambox_view.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/public/plugin_host.h"
#include "render/skia/canvas.h"
#include "tool/command.h"
#include "ui/views/primitives/button.h"
#include "ui/views/kernel/dpi.h"
#include "ui/views/primitives/label.h"
#include "ui/views/primitives/scroll_view.h"
#include "ui/views/kernel/theme.h"
#include "ui/views/kernel/widget.h"

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
                        AmboxView::Group* pan,
                        AmboxView::Group* edit,
                        AmboxView::Group* tools) {
  if (!catalog || !select || !pan || !edit || !tools) {
    return;
  }
  catalog->for_each([&](std::string_view id) {
    if (id.empty()) {
      return;
    }
    AmboxView::Group* target = nullptr;
    if (starts_with(id, "selection.") || id == "select" || id == "identify") {
      target = select;
    } else if (starts_with(id, "view.") || id == "pan") {
      target = pan;
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
  AmboxView::Group pan{"Pan", {}};
  AmboxView::Group edit{"Edit", {}};
  AmboxView::Group tools{"Tools", {}};

  for (tool::CommandCatalog* catalog : catalogs) {
    append_catalog_ids(catalog, &select, &pan, &edit, &tools);
  }

  ensure_item(&select.items, "select", "Select");
  ensure_item(&select.items, "identify", "Identify");
  ensure_item(&pan.items, "pan", "Pan");
  ensure_item(&edit.items, "edit.append.point", "Append point");
  ensure_item(&edit.items, "edit.append.linestring", "Append line");
  ensure_item(&edit.items, "edit.append.polygon", "Append polygon");
  ensure_item(&edit.items, "edit.undo", "Undo");
  ensure_item(&edit.items, "edit.cancel", "Cancel");

  std::vector<AmboxView::Group> groups;
  groups.push_back(std::move(select));
  groups.push_back(std::move(pan));
  groups.push_back(std::move(edit));
  if (!tools.items.empty()) {
    groups.push_back(std::move(tools));
  }
  return groups;
}

}  // namespace

class AmboxView::GroupBlock : public View {
 public:
  explicit GroupBlock(const AmboxView::Group& group, AmboxView* owner) {
    auto header = std::make_unique<Label>(group.name);
    header->set_preferred_size({160, 28});
    header_ = header.get();
    add_child(std::move(header));

    buttons_.reserve(group.items.size());
    for (const auto& item : group.items) {
      auto button = std::make_unique<Button>(item.label);
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
  set_preferred_size({180, 240});
  auto scroll = std::make_unique<ScrollView>();
  auto content = std::make_unique<View>();
  content_ = content.get();
  scroll->add_child(std::move(content));
  scroll_ = scroll.get();
  add_child(std::move(scroll));
  populate_from_plugin_host(nullptr);
}

AmboxView::~AmboxView() = default;

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
    const std::vector<tool::CommandCatalog*>& catalogs) {
  set_groups(groups_from_catalogs(catalogs));
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

void AmboxView::fire_command(const std::string& id) {
  if (handler_) {
    handler_(id);
  }
}

int AmboxView::measure_content_height(float scale) const {
  const int pad = dip_to_px(8, scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  const int gap = dip_to_px(2, scale);
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

void AmboxView::layout() {
  const Rect& b = bounds();
  const float scale =
      widget() ? widget()->device_scale_factor() : 1.f;
  const int pad = dip_to_px(8, scale);
  const int header_h = dip_to_px(28, scale);
  const int btn_h = dip_to_px(28, scale);
  const int gap = dip_to_px(2, scale);

  if (scroll_) {
    scroll_->set_bounds(b);
  }
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

void AmboxView::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
}

}  // namespace views
}  // namespace ui
