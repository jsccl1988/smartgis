// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_GIS_CATALOG_VIEW_H_
#define UI_VIEWS_GIS_CATALOG_VIEW_H_

#include <functional>
#include <string>
#include <vector>

#include "ui/views/gis/layer_tree.h"
#include "ui/views/kernel/view.h"

namespace ui {
namespace views {

class Label;
class TabStrip;
class TreeView;

// Left chrome catalog: title plus Layers / Datasources / Maps pages.
// Nodes are opaque string IDs; this widget never stores GIS pointers.
// Product hosts populate trees and handle set_command ids.
class CatalogView : public View {
 public:
  // One row in a datasource or map-doc tree. parent_id empty means root.
  struct CatalogNode {
    std::string id;
    std::string parent_id;
    std::string label;
    bool checked = false;
  };

  using Command = std::function<void(const std::string& command_id)>;

  CatalogView();

  void set_title(std::string title);
  const std::string& title() const;

  LayerTree* layer_tree() { return layer_tree_; }
  const LayerTree* layer_tree() const { return layer_tree_; }

  TreeView* source_tree() { return source_tree_; }
  const TreeView* source_tree() const { return source_tree_; }

  TreeView* map_tree() { return map_tree_; }
  const TreeView* map_tree() const { return map_tree_; }

  // Flat names become root nodes on the Datasources page (legacy helper).
  void set_source_names(const std::vector<std::string>& names);

  void set_source_tree(const std::vector<CatalogNode>& nodes);
  void set_map_docs(const std::vector<CatalogNode>& nodes);

  // Replace Layers page from a host layer list (open-map / CatalogCall mirror).
  // Non-empty |layers| clears demo rows. Empty |layers| installs demo fallback.
  void populate_layers(const std::vector<LayerTree::LayerDesc>& layers);

  // Install the single demo row used when no real layer list is available.
  void populate_demo_layers();

  // True while Layers page still shows the demo fallback row.
  bool using_demo_layers() const { return using_demo_layers_; }

  // Host callback for context-menu command ids such as catalog.layer.create.
  void set_command(Command fn);

  TabStrip* source_tabs() { return tabs_; }
  const TabStrip* source_tabs() const { return tabs_; }

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  void populate_tree(TreeView* tree, const std::vector<CatalogNode>& nodes);
  void fire_command(const std::string& command_id);
  void show_layer_menu(Point screen);
  void show_source_menu(Point screen);
  void show_map_menu(Point screen);
  HWND owner_hwnd() const;

  Label* title_ = nullptr;
  TabStrip* tabs_ = nullptr;
  LayerTree* layer_tree_ = nullptr;
  TreeView* source_tree_ = nullptr;
  TreeView* map_tree_ = nullptr;
  Command command_;
  bool using_demo_layers_ = false;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_GIS_CATALOG_VIEW_H_
