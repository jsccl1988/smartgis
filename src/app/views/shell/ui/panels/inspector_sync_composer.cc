// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/ui/panels/inspector_sync_composer.h"
#include "app/views/shell/ui/browser_view.h"

#include "app/views/shell/browser/browser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/shell/browser/commands/app_commands.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/runtime/host/registry/registry.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "vista/domain/atmosphere/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/edit/session.h"
#include "gis/carto/tile/tile_map_layer.h"
#include "gis/carto/tile/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/add_basemap_dialog.h"
#include "ui/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/inspect/attribute_schema_dialog.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/catalog/create_datasource_dialog.h"
#include "ui/gis/catalog/create_layer_dialog.h"
#include "ui/gis/catalog/create_map_dialog.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/input_text_dialog.h"
#include "ui/gis/catalog/layer_tree.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/analysis/result_playback_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/style/symbology_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

// FeatureInfo / AttributeTable sync and selection feedback.

// FeatureInfo / AttributeTable / playback sync and edit feedback.
InspectorSyncComposer::InspectorSyncComposer(BrowserView* host) : host_(host) {}

void InspectorSyncComposer::sync_inspectors_from_scene() {
  if (host_->attribute_table_) {
    std::vector<std::string> cols;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> tokens;
    host_->browser_->document()->fill_attribute_rows(&cols, &rows, &tokens);
    host_->attribute_table_->set_columns(cols);
    host_->attribute_table_->set_rows(rows);
    host_->attribute_table_->set_row_tokens(std::move(tokens));
  }
  if (host_->feature_info_) {
    if (const MapScene::Feature* f = host_->browser_->document()->selected_feature()) {
      ui::views::FeatureInfo::Hit hit;
      hit.feature_id = MapScene::feature_token(f->id);
      for (const MapScene::Layer& layer : host_->browser_->document()->layers()) {
        for (const MapScene::Feature& candidate : layer.features) {
          if (std::memcmp(candidate.id.bytes, f->id.bytes,
                          sizeof(f->id.bytes)) == 0 &&
              candidate.id.len == f->id.len) {
            hit.layer_name = layer.name;
            break;
          }
        }
        if (!hit.layer_name.empty()) {
          break;
        }
      }
      switch (f->kind) {
        case MapScene::GeomKind::kLine:
          hit.geometry_type = "Line";
          break;
        case MapScene::GeomKind::kPolygon:
          hit.geometry_type = "Polygon";
          break;
        case MapScene::GeomKind::kText:
          hit.geometry_type = "Text";
          break;
        case MapScene::GeomKind::kPoint:
        default:
          hit.geometry_type = "Point";
          break;
      }
      const double map_scale =
          host_->browser_->view_frame() ? host_->browser_->view_frame()->scale() : 8.0;
      std::vector<std::pair<std::string, std::string>> pairs;
      host_->browser_->document()->fill_feature_info_fields(*f, &pairs, hit.layer_name,
                                                     map_scale);
      hit.fields.reserve(pairs.size());
      for (auto& p : pairs) {
        hit.fields.push_back({std::move(p.first), std::move(p.second)});
      }
      // Single-selection sync; identify_at overwrites with multi-hit when needed.
      host_->feature_info_->set_hits({std::move(hit)}, 0);
    }
  }
  host_->sync_selection_panel_from_scene();
  host_->sync_legend_panel_from_scene();
  host_->sync_layer_properties_from_scene();
  host_->sync_result_playback_from_session();
}


void InspectorSyncComposer::sync_result_playback_from_session() {
  // Require the panel to be under this shell Widget. Skip during early
  // wire_map_scene if the panel pointer is skewed (stale shell_ui .obj) or
  // not yet reparented 鈥?Slider::set_value 鈫?schedule_paint on a garbage
  // host_->widget_ was STATUS_HEAP_CORRUPTION / AV at init_shell.
  if (!host_->result_playback_panel_ ||
      host_->result_playback_panel_->widget() != &host_->widget_) {
    return;
  }
  auto& session = host_->browser_->analysis_playback();
  host_->result_playback_panel_->set_frame_range(session.frame_count());
  host_->result_playback_panel_->set_frame_index(session.frame_index());
  host_->result_playback_panel_->set_looping(session.looping());
  host_->result_playback_panel_->set_playing(session.playing());
  if (session.frame_count() > 0) {
    host_->result_playback_panel_->set_status_text(
        session.product() == AnalysisProduct::kTraffic
            ? "traffic"
            : session.product() == AnalysisProduct::kFlood
                  ? "flood"
                  : session.product() == AnalysisProduct::kStormSurge
                        ? "stormsurge"
                        : session.product() == AnalysisProduct::kOrthogrid
                              ? "orthogrid"
                              : session.product() ==
                                        AnalysisProduct::kOrthogrid3d
                                    ? "orthogrid3d"
                                    : "session");
  } else {
    host_->result_playback_panel_->set_status_text("(no session)");
  }
}


void InspectorSyncComposer::wire_edit_feedback() {
  if (host_->feature_info_) {
    // Multi-hit </>/arrows: keep map selection aligned with the active Hit.
    host_->feature_info_->set_hit_changed([this](size_t /*index*/) {
      if (!host_->feature_info_) {
        return;
      }
      const std::string& token = host_->feature_info_->feature_id();
      if (token.empty()) {
        return;
      }
      const content::FeatureId id = MapScene::feature_id_from_token(token);
      if (!host_->browser_->document()->select_feature(id)) {
        return;
      }
      host_->sync_selection_panel_from_scene();
      host_->invalidate_map_overlays();
      host_->set_status_message("Selected " + token);
    });
  }
  // Showcase / self-test set SMT_SKIP_AMBOX_CATALOG. Edit subscriptions are not
  // required for BMP export. A skewed Browser/MapSession layout (stale
  // shell_browser .obj under parallel ninja) makes edit_host() return
  // 0xCD-filled garbage 鈫?STATUS_HEAP_CORRUPTION in ViewHost::events().
  if (const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
      skip && skip[0] != '\0' && skip[0] != '0') {
    return;
  }
  content::ViewHost* host = host_->browser_->edit_host();
  if (!host) {
    return;
  }
  const auto addr = reinterpret_cast<uintptr_t>(host);
  if (addr < 0x10000u || (addr & 0xffu) == 0xcdu || (addr >> 24) == 0xcdu) {
    return;
  }
  content::EventBus* events = host->events();
  if (!events) {
    return;
  }
  *host_->browser_->selection_sub() = events->subscribe<content::SelectionChanged>(
      [this](const content::SelectionChanged& ev) {
        if (ev.ids.empty()) {
          // Prefer MapScene hit-test result from draft_observer; only clear
          // when the workspace explicitly cleared selection.
          if (!host_->browser_->document()->selected_feature()) {
            host_->set_status_message("Selection cleared");
            if (host_->feature_info_) {
              host_->feature_info_->clear();
            }
          }
          return;
        }
        host_->set_status_message("Selected " + std::to_string(ev.ids.size()) +
                           " feature(s)");
      });
  *host_->browser_->edit_sub() = events->subscribe<content::EditCommitted>(
      [this](const content::EditCommitted& ev) {
        const char* op = "modify";
        if (ev.op == content::EditCommitted::Op::kAppend) {
          op = "append";
        } else if (ev.op == content::EditCommitted::Op::kDelete) {
          op = "delete";
        }
        host_->set_status_message(std::string("Committed ") + op);
        host_->sync_inspectors_from_scene();
        host_->invalidate_map_overlays();
      });
  *host_->browser_->extent_sub() = events->subscribe<content::ExtentChanged>(
      [this](const content::ExtentChanged& ev) {
        host_->browser_->OnExtentChanged(ev.view_id, ev.extent);
      });

  if (host_->attribute_table_) {
    host_->attribute_table_->set_on_cell_commit(
        [this](const std::string& feature_token, const std::string& field,
               const std::string& value) {
          if (!host_->browser_->document()->update_feature_field(feature_token, field, value)) {
            host_->set_status_message("Attribute edit failed: " + field);
            return false;
          }
          content::ViewHost* host = host_->active_view_host();
          if (host && host->edits()) {
            gis::FeatureMutation mutation;
            mutation.op = gis::EditOp::kModify;
            mutation.id = MapScene::feature_id_from_token(feature_token);
            host->edits()->commit(mutation);
          }
          host_->set_status_message("Updated " + field + "=" + value);
          host_->sync_inspectors_from_scene();
          host_->invalidate_map_overlays();
          return true;
        });
    host_->attribute_table_->set_selected([this](int row) {
      if (!host_->attribute_table_) {
        return;
      }
      const std::string& token = host_->attribute_table_->row_token(row);
      if (token.empty()) {
        return;
      }
      const content::FeatureId id = MapScene::feature_id_from_token(token);
      if (!host_->browser_->document()->select_feature(id)) {
        return;
      }
      host_->sync_inspectors_from_scene();
      host_->invalidate_map_overlays();
      host_->set_status_message("Selected " + token);
    });
  }
}


}  // namespace app
