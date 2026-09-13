// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/catalog_view.h"

#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "render/skia/canvas.h"
#include "ui/views/context_menu.h"
#include "ui/views/label.h"
#include "ui/views/layer_tree.h"
#include "ui/views/layout.h"
#include "ui/views/tab_strip.h"
#include "ui/views/theme.h"
#include "ui/views/tree_view.h"
#include "ui/views/widget.h"

namespace ui {
namespace views {

namespace {

struct CatalogMenuEntry {
  const char* command_id = nullptr;
  const char* label = nullptr;
  bool separator = false;
};

std::vector<MenuItem> make_menu(const CatalogMenuEntry* entries,
                                size_t count,
                                const std::function<void(const std::string&)>&
                                    fire) {
  std::vector<MenuItem> items;
  items.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const CatalogMenuEntry& entry = entries[i];
    MenuItem item;
    item.separator = entry.separator;
    item.enabled = true;
    if (entry.separator) {
      items.push_back(std::move(item));
      continue;
    }
    item.label = entry.label ? entry.label : "";
    const std::string id = entry.command_id ? entry.command_id : "";
    item.invoke = [fire, id]() {
      if (fire) {
        fire(id);
      }
    };
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace

CatalogView::CatalogView() {
  set_preferred_size({240, 0});
  auto box = std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  auto title = std::make_unique<Label>("Catalog");
  title->set_preferred_size({240, 36});
  title_ = title.get();

  auto tabs = std::make_unique<TabStrip>();
  tabs->set_preferred_size({240, 280});
  tabs_ = tabs.get();

  auto layers = std::make_unique<LayerTree>();
  layer_tree_ = layers.get();
  layer_tree_->set_context_requested(
      [this](const std::string&, Point screen) { show_layer_menu(screen); });

  auto sources = std::make_unique<TreeView>();
  source_tree_ = sources.get();
  source_tree_->set_context_requested(
      [this](const TreeView::NodeId&, Point screen) {
        show_source_menu(screen);
      });

  auto maps = std::make_unique<TreeView>();
  map_tree_ = maps.get();
  map_tree_->set_context_requested(
      [this](const TreeView::NodeId&, Point screen) { show_map_menu(screen); });

  tabs_->add_tab("Layers", std::move(layers));
  tabs_->add_tab("Datasources", std::move(sources));
  tabs_->add_tab("Maps", std::move(maps));

  box->set_flex_for_view(tabs_, 1);
  set_layout_manager(std::move(box));
  add_child(std::move(title));
  add_child(std::move(tabs));
}

void CatalogView::set_title(std::string title) {
  if (title_) {
    title_->set_text(std::move(title));
  }
}

const std::string& CatalogView::title() const {
  static const std::string kEmpty;
  return title_ ? title_->text() : kEmpty;
}

void CatalogView::set_source_names(const std::vector<std::string>& names) {
  std::vector<CatalogNode> nodes;
  nodes.reserve(names.size());
  for (size_t i = 0; i < names.size(); ++i) {
    CatalogNode node;
    node.id = names[i].empty() ? std::to_string(i) : names[i];
    node.parent_id.clear();
    node.label = names[i];
    node.checked = false;
    nodes.push_back(std::move(node));
  }
  set_source_tree(nodes);
}

void CatalogView::set_source_tree(const std::vector<CatalogNode>& nodes) {
  populate_tree(source_tree_, nodes);
}

void CatalogView::set_map_docs(const std::vector<CatalogNode>& nodes) {
  populate_tree(map_tree_, nodes);
}

void CatalogView::set_command(Command fn) {
  command_ = std::move(fn);
}

void CatalogView::populate_tree(TreeView* tree,
                                const std::vector<CatalogNode>& nodes) {
  if (!tree) {
    return;
  }
  tree->clear();
  for (const CatalogNode& node : nodes) {
    tree->add_node(node.parent_id, node.id, node.label, node.checked);
  }
}

void CatalogView::fire_command(const std::string& command_id) {
  if (command_) {
    command_(command_id);
  }
}

HWND CatalogView::owner_hwnd() const {
  return widget() ? widget()->hwnd() : nullptr;
}

void CatalogView::show_layer_menu(Point screen) {
  static const CatalogMenuEntry kEntries[] = {
      {"catalog.layer.append", "Append layer"},
      {"catalog.layer.add_basemap", "Add online basemap"},
      {"catalog.layer.remove", "Remove layer"},
      {"catalog.layer.active", "Set active"},
      {"catalog.layer.property", "Properties"},
      {nullptr, nullptr, true},
      {"catalog.layer.attstruct", "Attribute structure"},
      {"catalog.layer.recalc_mbr", "Recalc MBR"},
  };
  show_context_menu(
      owner_hwnd(), screen,
      make_menu(kEntries, std::size(kEntries),
                [this](const std::string& id) { fire_command(id); }));
}

void CatalogView::show_source_menu(Point screen) {
  static const CatalogMenuEntry kEntries[] = {
      {"catalog.layer.create", "Create layer"},
      {"catalog.layer.delete", "Delete layer"},
      {"catalog.layer.load_shp", "Load shapefile"},
      {"catalog.layer.load_image", "Load image"},
      {nullptr, nullptr, true},
      {"catalog.ds.create", "Create datasource"},
      {"catalog.ds.append", "Append datasource"},
      {"catalog.ds.delete", "Delete datasource"},
      {"catalog.ds.set_active", "Set active"},
      {"catalog.ds.property", "Properties"},
  };
  show_context_menu(
      owner_hwnd(), screen,
      make_menu(kEntries, std::size(kEntries),
                [this](const std::string& id) { fire_command(id); }));
}

void CatalogView::show_map_menu(Point screen) {
  static const CatalogMenuEntry kEntries[] = {
      {"catalog.map.create", "New map"},
      {"catalog.map.open", "Open map"},
      {"catalog.map.save", "Save map"},
      {"catalog.map.save_as", "Save map as"},
      {"catalog.map.close", "Close map"},
      {nullptr, nullptr, true},
      {"catalog.layer.append", "Append layer"},
      {"catalog.layer.add_basemap", "Add online basemap"},
      {"catalog.layer.remove", "Remove layer"},
      {"catalog.layer.active", "Set active"},
      {"catalog.layer.property", "Properties"},
  };
  show_context_menu(
      owner_hwnd(), screen,
      make_menu(kEntries, std::size(kEntries),
                [this](const std::string& id) { fire_command(id); }));
}

void CatalogView::paint_self(render::skia::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  canvas->fill_rect(b.x, b.y, b.width, 36, t.accent);
}

}  // namespace views
}  // namespace ui
