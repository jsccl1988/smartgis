// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/ambox_view.h"

#include <utility>

#include "content/public/plugin_host.h"
#include "render/skia/canvas.h"
#include "ui/views/button.h"
#include "ui/views/label.h"
#include "ui/views/theme.h"

namespace ui {
namespace views {

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
  // contribute_command / contribute_menu store data, but PluginHost and
  // CommandCatalog expose no list API on content/public.
  if (host && host->commands()) {
    (void)host->commands();
  }
  set_groups({
      {"Select", {{"select", "Select"}}},
      {"Pan", {{"pan", "Pan"}}},
      {"Identify", {{"identify", "Identify"}}},
  });
}

void AmboxView::rebuild() {
  remove_all_children();
  blocks_.clear();
  blocks_.reserve(groups_.size());
  for (const auto& group : groups_) {
    auto block = std::make_unique<GroupBlock>(group, this);
    blocks_.push_back(block.get());
    add_child(std::move(block));
  }
  layout();
}

void AmboxView::fire_command(const std::string& id) {
  if (handler_) {
    handler_(id);
  }
}

void AmboxView::layout() {
  const Rect& b = bounds();
  int y = b.y;
  for (auto& block : blocks_) {
    if (!block) {
      continue;
    }
    const int header_h = 28;
    const int btn_h = 28;
    const int h =
        header_h + btn_h * static_cast<int>(block->button_count());
    block->set_bounds({b.x, y, b.width, h});
    if (View* header = block->header()) {
      header->set_bounds({b.x + 4, y, b.width > 8 ? b.width - 8 : 0, header_h});
    }
    int row_y = y + header_h;
    for (size_t i = 0; i < block->button_count(); ++i) {
      if (View* button = block->button_at(i)) {
        button->set_bounds(
            {b.x + 8, row_y, b.width > 16 ? b.width - 16 : 0, btn_h});
      }
      row_y += btn_h;
    }
    y += h;
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
