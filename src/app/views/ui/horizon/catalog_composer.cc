// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/catalog_composer.h"

#include "app/views/ui/browser_view.h"

#include <functional>
#include <string>
#include <vector>

#include "app/views/browser/browser.h"
#include "content/browser/session/browser_session.h"
#include "content/public/map_contents.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/map/viewport/draw_host.h"

namespace app {
namespace detail {
std::string json_escape(const std::string& text);
void catalog_call(content::MapContents* session, const std::string& json);
}  // namespace detail

CatalogComposer::CatalogComposer(BrowserView* host) : host_(host) {}

void CatalogComposer::wire_catalog() {
  if (!host_->catalog_ || !host_->catalog_->layer_tree()) {
    return;
  }
  host_->sync_catalog_from_scene();
  // Bare launch seeds china_city (same pack as harness). Label Sources/Maps
  // to match --ui-showcase=shell so interactive fix loops see china_city.
  if (host_->browser_ &&
      host_->browser_->session().document_has_china_extent()) {
    host_->catalog_->set_source_names({"china_city"});
    host_->catalog_->set_map_docs({{"china_city", "", "China", false}});
  } else {
    host_->catalog_->set_source_names({"Memory"});
    host_->catalog_->set_map_docs({{"map.untitled", "", "Untitled map", false}});
  }
  host_->catalog_->set_command(
      [this](const std::string& id) { host_->browser_->on_catalog_command(id); });
  host_->catalog_->layer_tree()->set_visible_changed(
      [this](const std::string& id, bool visible) {
        host_->browser_->session().set_layer_visible(id, visible);
        content::MapContents* session = host_->active_map()
                                            ? host_->active_map()->map_contents()
                                            : host_->browser_->map_session();
        detail::catalog_call(
            session, std::string("{\"op\":\"set_visible\",\"id\":\"") +
                         detail::json_escape(id) + "\",\"visible\":" +
                         (visible ? "true" : "false") + "}");
        // Visibility is in ContentFingerprint; presenter drops MapIR only when
        // the hash moved (stale Land/jet must not StaticReuse).
        host_->browser_->session().invalidate_map2d_frame_cache();
        host_->invalidate_map_overlays();
        host_->sync_inspectors_from_scene();
        if (host_->status_bar_) {
          host_->status_bar_->set_message(std::string("Layer ") + id +
                                   (visible ? ": visible" : ": hidden"));
        }
      });
  host_->catalog_->layer_tree()->set_selection_changed([this](const std::string& id) {
    host_->browser_->session().select_layer(id);
    content::MapContents* session = host_->active_map() ? host_->active_map()->map_contents()
                                                 : host_->browser_->map_session();
    detail::catalog_call(session,
                         std::string("{\"op\":\"select_layer\",\"id\":\"") +
                             detail::json_escape(id) + "\"}");
    host_->sync_inspectors_from_scene();
    host_->invalidate_map_overlays();
    if (host_->status_bar_) {
      host_->status_bar_->set_message("Active layer: " + id);
    }
  });
}


void CatalogComposer::sync_catalog_from_scene() {
  if (!host_->catalog_ || !host_->browser_ || !host_->browser_->document()) {
    return;
  }
  auto to_views_kind = [](content::LayerKind k) {
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
  };
  std::function<ui::views::LayerTree::LayerDesc(const content::LayerDesc&)>
      convert = [&](const content::LayerDesc& d) {
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
        } else {
          row.name = d.name;
        }
        row.visible = d.visible;
        row.active = d.active;
        row.kind = to_views_kind(d.kind);
        row.expanded = d.expanded;
        row.children.reserve(d.children.size());
        for (const content::LayerDesc& child : d.children) {
          row.children.push_back(convert(child));
        }
        return row;
      };
  std::vector<ui::views::LayerTree::LayerDesc> layers;
  const std::vector<content::LayerDesc> descs =
      host_->browser_->session().document_layer_descs();
  layers.reserve(descs.size());
  for (const content::LayerDesc& d : descs) {
    layers.push_back(convert(d));
  }
  host_->catalog_->populate_layers(layers);
}


}  // namespace app
