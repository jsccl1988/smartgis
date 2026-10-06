// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/gis/catalog/catalog_view.h"

#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ui/gfx/canvas/canvas.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/text/label.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/primitives/collection/tree_view.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/markup/loader/markup_loader.h"

#include <algorithm>

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
  MarkupRoot loaded = load_markup("catalog/catalog_view.ui.xml");
  if (!loaded.ok()) {
    set_preferred_size({240, 0});
    return;
  }
  title_ = loaded.ids.find_as<Label>("title");
  View* tabs_host = loaded.ids.find("tabs_host");

  // Keep Map/Data/3D and Layers/Sources/Maps on one horizontal band — a separate
  // Catalog title row used to push source tabs down into the map horizon.
  // Preferred width must fit three tab labels (Layers/Sources/Maps) without
  // clipping into the Map tab accent (visual_review: Sources obscured).
  if (title_) {
    title_->set_visible(false);
    title_->set_preferred_size({0, 0});
  }

  auto tabs = std::make_unique<TabStrip>();
  // Preferred width seeds the catalog_map splitter; Yoga children stretch to
  // the host so a temporarily narrow pane does not overflow (layout-fail).
  tabs->set_preferred_size({0, 0});
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
  tabs_->add_tab("Sources", std::move(sources));
  tabs_->add_tab("Maps", std::move(maps));

  if (tabs_host) {
    tabs_host->set_preferred_size({0, 0});
    tabs_host->set_layout_manager(std::make_unique<FillLayout>());
    tabs_host->add_child(std::move(tabs));
  }

  auto fill = std::make_unique<FillLayout>();
  set_layout_manager(std::move(fill));
  loaded.root->set_preferred_size({0, 0});
  add_child(std::move(loaded.root));
  set_preferred_size({300, 0});
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

void CatalogView::relayout_layers_page() {
  if (tabs_) {
    tabs_->layout();
  }
  layout();
  if (Widget* w = widget()) {
    w->layout_contents();
  }
  if (layer_tree_) {
    layer_tree_->layout();
    layer_tree_->schedule_paint();
  }
  schedule_paint();
}

void CatalogView::populate_layers(
    const std::vector<LayerTree::LayerDesc>& layers) {
  if (!layer_tree_) {
    return;
  }
  // Real-data policy: empty catalog stays empty (no demo Streets/Imagery).
  layer_tree_->set_layers(layers);
  using_demo_layers_ = false;
  // TabStrip body may still have been 0×0 when set_layers first laid out
  // (ui.shell #2 empty Layers). Force a shell layout so rows get bounds.
  relayout_layers_page();
}

void CatalogView::populate_demo_layers() {
  if (!layer_tree_) {
    return;
  }
  // Two-level demo: group with vector/raster children + a root leaf. Proves
  // expand chevron, type glyphs, and indent without a live MapScene tree.
  LayerTree::LayerDesc streets;
  streets.id = "layer.demo.streets";
  streets.name = "Streets";
  streets.visible = true;
  streets.kind = LayerKind::kVector;

  LayerTree::LayerDesc imagery;
  imagery.id = "layer.demo.imagery";
  imagery.name = "Imagery";
  imagery.visible = true;
  imagery.kind = LayerKind::kRaster;

  LayerTree::LayerDesc basemap;
  basemap.id = "layer.demo.basemap";
  basemap.name = "Basemap";
  basemap.visible = true;
  basemap.active = true;
  basemap.kind = LayerKind::kGroup;
  basemap.expanded = true;
  basemap.children.push_back(std::move(streets));
  basemap.children.push_back(std::move(imagery));

  LayerTree::LayerDesc notes;
  notes.id = "layer.demo.notes";
  notes.name = "Annotations";
  notes.visible = true;
  notes.kind = LayerKind::kVector;

  layer_tree_->set_layers({std::move(basemap), std::move(notes)});
  using_demo_layers_ = true;
  relayout_layers_page();
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
      {"catalog.layer.view", "View"},
      {nullptr, nullptr, true},
      {"catalog.layer.append", "Append layer"},
      {"catalog.layer.add_basemap", "Add online basemap"},
      {"catalog.layer.remove", "Remove layer"},
      {"catalog.layer.active", "Set active"},
      {"catalog.layer.move_up", "Move up"},
      {"catalog.layer.move_down", "Move down"},
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
      {"catalog.layer.view", "View"},
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

void CatalogView::paint_self(ui::gfx::Canvas* canvas) {
  if (!canvas) {
    return;
  }
  const Theme& t = Theme::current();
  const Rect& b = bounds();
  canvas->fill_rect(b.x, b.y, b.width, b.height, t.panel_bg);
  const float scale = widget() ? widget()->device_scale_factor() : 1.f;
  const int hair = std::max(1, dip_to_px(1, scale));
  // Right seam against the map column (Pro/QGIS catalog dock edge).
  canvas->fill_rect(b.right() - hair, b.y, hair, b.height, t.panel_header);
  if (!title_ || !title_->is_visible()) {
    return;
  }
  const int header_h =
      title_->bounds().height > 0 ? title_->bounds().height : dip_to_px(28, scale);
  if (header_h > 0) {
    canvas->fill_rect(b.x, b.y, b.width, header_h, t.panel_header);
  }
}

}  // namespace views
}  // namespace ui
