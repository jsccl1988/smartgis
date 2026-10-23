// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/catalog_composer.h"

#include "app/views/ui/browser_view.h"

#include <functional>
#include <string>
#include <vector>

#include "app/views/browser/browser.h"
#include "app/views/ui/horizon/detail/catalog_layer_filter.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_contents.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {
namespace detail {
std::string json_escape(const std::string& text);
void catalog_call(content::GisContents* session, const std::string& json);
}  // namespace detail

namespace {

ui::views::LayerKind to_views_kind(content::LayerKind k) {
  switch (k) {
    case content::LayerKind::kGroup:
      return ui::views::LayerKind::kGroup;
    case content::LayerKind::kVector:
      return ui::views::LayerKind::kVector;
    case content::LayerKind::kRaster:
      return ui::views::LayerKind::kRaster;
    case content::LayerKind::kUnknown:
    default:
      return ui::views::LayerKind::kUnknown;
  }
}

ui::views::LayerTree::LayerDesc convert_desc(const content::LayerDesc& d) {
  ui::views::LayerTree::LayerDesc row;
  row.id = d.id;
  // china_city PLPT stems are short (area/line/point/text); show product
  // labels so the Layers panel stays legible on dark horizon.
  if (d.name == "area") {
    row.name = "Land";
  } else if (d.name == "line") {
    row.name = "Lines";
  } else if (d.name == "point") {
    row.name = "Points";
  } else if (d.name == "text") {
    row.name = "Labels";
  } else if (d.name == "hillshade" || d.id == "style.hillshade") {
    row.name = "Hillshade";
  } else {
    row.name = d.name;
  }
  row.visible = d.visible;
  row.active = d.active;
  row.kind = to_views_kind(d.kind);
  row.expanded = d.expanded;
  row.children.reserve(d.children.size());
  for (const content::LayerDesc& child : d.children) {
    row.children.push_back(convert_desc(child));
  }
  return row;
}

}  // namespace

CatalogComposer::CatalogComposer(BrowserView* host) : host_(host) {
  wire_baton_2d_ = {this, 0};
  wire_baton_3d_ = {this, 1};
}

void CatalogComposer::command_trampoline(void* baton, const char* id) {
  auto* b = static_cast<ModeBaton*>(baton);
  if (b && b->self) {
    b->self->on_catalog_command(id);
  }
}

void CatalogComposer::visible_trampoline(void* baton,
                                         const char* id,
                                         bool visible) {
  auto* b = static_cast<ModeBaton*>(baton);
  if (b && b->self) {
    b->self->on_layer_visible(b->mode_index, id, visible);
  }
}

void CatalogComposer::selection_trampoline(void* baton, const char* id) {
  auto* b = static_cast<ModeBaton*>(baton);
  if (b && b->self) {
    b->self->on_layer_selected(b->mode_index, id);
  }
}

void CatalogComposer::on_catalog_command(const char* id) {
  if (!host_ || !host_->browser_ || !id) {
    return;
  }
  host_->browser_->on_catalog_command(id);
}

void CatalogComposer::apply_active_catalog(int mode_index) {
  if (mode_index == 1 && host_->catalog_3d_) {
    host_->catalog_ = host_->catalog_3d_;
  } else if (host_->catalog_2d_) {
    host_->catalog_ = host_->catalog_2d_;
  }
}

void CatalogComposer::on_map_tab_changed(int index) {
  if (!host_ || syncing_mode_) {
    return;
  }
  const int i = index <= 0 ? 0 : 1;
  syncing_mode_ = true;
  apply_active_catalog(i);
  if (host_->catalog_mode_tabs_ && host_->catalog_mode_tabs_->active() != i) {
    host_->catalog_mode_tabs_->set_active(i);
  }
  syncing_mode_ = false;
}

void CatalogComposer::on_catalog_mode_changed(int index) {
  if (!host_ || syncing_mode_) {
    return;
  }
  const int i = index <= 0 ? 0 : 1;
  syncing_mode_ = true;
  apply_active_catalog(i);
  // switch_map_tab owns present attach; set_active alone is not enough.
  // Nested on_map_tab_changed is suppressed by syncing_mode_.
  if (host_->map_tabs_ && host_->map_tabs_->active() != i) {
    host_->switch_map_tab(i);
  }
  syncing_mode_ = false;
}

void CatalogComposer::on_layer_visible(int mode_index,
                                       const char* id,
                                       bool visible) {
  if (!host_ || !host_->browser_ || !id) {
    return;
  }
  host_->browser_->session().set_layer_visible(id, visible);
  content::GisContents* session = host_->active_map()
                                      ? host_->active_map()->gis_contents()
                                      : host_->browser_->map_session();
  detail::catalog_call(
      session, std::string("{\"op\":\"set_visible\",\"id\":\"") +
                   detail::json_escape(id) + "\",\"visible\":" +
                   (visible ? "true" : "false") + "}");
  if (mode_index == 1) {
    host_->invalidate_native_scene();
    if (host_->map_scene_) {
      host_->map_scene_->request_frame();
    }
  } else {
    host_->browser_->session().invalidate_map2d_frame_cache();
    host_->invalidate_map_overlays();
  }
  host_->sync_catalog_from_scene();
  host_->sync_inspectors_from_scene();
  if (host_->status_bar_) {
    host_->status_bar_->set_message(std::string("Layer ") + id +
                                    (visible ? ": visible" : ": hidden"));
  }
}

void CatalogComposer::on_layer_selected(int mode_index, const char* id) {
  if (!host_ || !host_->browser_ || !id) {
    return;
  }
  host_->browser_->session().select_layer(id);
  content::GisContents* session =
      host_->active_map() ? host_->active_map()->gis_contents()
                          : host_->browser_->map_session();
  detail::catalog_call(session,
                       std::string("{\"op\":\"select_layer\",\"id\":\"") +
                           detail::json_escape(id) + "\"}");
  host_->sync_inspectors_from_scene();
  if (mode_index == 1) {
    host_->invalidate_native_scene();
    if (host_->map_scene_) {
      host_->map_scene_->request_frame();
    }
  } else {
    host_->invalidate_map_overlays();
  }
  if (host_->status_bar_) {
    host_->status_bar_->set_message(std::string("Active layer: ") + id);
  }
}

void CatalogComposer::wire_one(ui::views::CatalogView* catalog,
                               ModeBaton* baton) {
  if (!catalog || !catalog->layer_tree() || !baton) {
    return;
  }
  const bool scene3d = baton->mode_index == 1;
  if (!scene3d) {
    if (host_->browser_ &&
        host_->browser_->session().document_has_land_surface()) {
      const char* name = "china_city";
      catalog->set_source_names_c(&name, 1);
      catalog->clear_map_docs();
      catalog->add_map_doc("china_city", "", "China", false);
    } else {
      const char* name = "Memory";
      catalog->set_source_names_c(&name, 1);
      catalog->clear_map_docs();
      catalog->add_map_doc("map.untitled", "", "Untitled map", false);
    }
  } else {
    catalog->set_source_names_c(nullptr, 0);
    catalog->clear_map_docs();
  }
  catalog->set_command_fn(&CatalogComposer::command_trampoline, baton);
  catalog->layer_tree()->set_visible_changed_fn(
      &CatalogComposer::visible_trampoline, baton);
  catalog->layer_tree()->set_selection_changed_fn(
      &CatalogComposer::selection_trampoline, baton);
}

void CatalogComposer::wire_catalog() {
  wire_one(host_->catalog_2d_ ? host_->catalog_2d_ : host_->catalog_,
           &wire_baton_2d_);
  if (host_->catalog_3d_ && host_->catalog_3d_ != host_->catalog_2d_) {
    wire_one(host_->catalog_3d_, &wire_baton_3d_);
  }
  if (host_->catalog_mode_tabs_) {
    host_->catalog_mode_tabs_->set_change_fn(
        [](void* baton, int i) {
          static_cast<CatalogComposer*>(baton)->on_catalog_mode_changed(i);
        },
        this);
  }
  apply_active_catalog(host_->catalog_mode_tabs_
                           ? host_->catalog_mode_tabs_->active()
                           : 0);
  host_->sync_catalog_from_scene();
}

void CatalogComposer::populate_one(ui::views::CatalogView* catalog,
                                   int mode_index) {
  if (!catalog || !host_->browser_ || !host_->browser_->document()) {
    return;
  }
  const detail::CatalogViewMode mode = mode_index == 1
                                           ? detail::CatalogViewMode::kScene3d
                                           : detail::CatalogViewMode::kMap2d;
  const std::vector<content::LayerDesc> descs =
      detail::filter_layer_descs_for_mode(
          host_->browser_->session().document_layer_descs(), mode);
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  layers.reserve(descs.size());
  for (const content::LayerDesc& d : descs) {
    layers.push_back(convert_desc(d));
  }
  catalog->populate_layers(layers);
}

void CatalogComposer::sync_catalog_from_scene() {
  if (!host_->browser_ || !host_->browser_->document()) {
    return;
  }
  if (host_->catalog_2d_) {
    populate_one(host_->catalog_2d_, 0);
  } else if (host_->catalog_) {
    populate_one(host_->catalog_, 0);
  }
  if (host_->catalog_3d_) {
    populate_one(host_->catalog_3d_, 1);
  }
}

}  // namespace app
