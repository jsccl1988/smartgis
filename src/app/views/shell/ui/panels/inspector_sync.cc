// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

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
#include "app/views/camera/map_host_extent.h"
#include "app/views/camera/view_frame.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/browser/commands/view_commands.h"
#include "plugin/product/dem/dem_commands.h"
#include "plugin/product/orthogrid/commands.h"
#include "plugin/runtime/host/registry.h"
#include "content/public/catalog_layers.h"
#include "content/public/map_contents.h"
#include "content/public/map_types.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "gis/model/edit/session/edit_session.h"
#include "gis/present/tile/provider/tile_map_layer.h"
#include "gis/present/tile/provider/tile_provider.h"
#include "tool/nav/camera_nav.h"
#include "tool/command/command.h"
#include "tool/draft/draft.h"
#include "tool/workspace/workspace.h"
#include "ui/views/dialogs/gis/add_basemap_dialog.h"
#include "ui/views/gis/shell/ambox_view.h"
#include "plugin/runtime/processing/builtin_ops.h"
#include "plugin/runtime/processing/ops_runner.h"
#include "ui/views/gis/panel/atmosphere_panel.h"
#include "ui/views/dialogs/gis/att_struct_dialog.h"
#include "ui/views/gis/inspect/attribute_table.h"
#include "ui/views/gis/catalog/catalog_view.h"
#include "ui/views/dialogs/gis/create_datasource_dialog.h"
#include "ui/views/dialogs/gis/create_layer_dialog.h"
#include "ui/views/dialogs/gis/create_map_dialog.h"
#include "ui/views/gis/inspect/feature_info.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/input_text_dialog.h"
#include "ui/views/gis/catalog/layer_tree.h"
#include "ui/views/gis/panel/processing_panel.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/menu/context_menu.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/layout/splitter.h"
#include "ui/views/gis/shell/status_bar.h"
#include "ui/views/primitives/collection/tab_strip.h"
#include "ui/views/kernel/view/view.h"

namespace app {

// FeatureInfo / AttributeTable sync and selection feedback.

void BrowserView::sync_inspectors_from_scene() {
  if (attribute_table_) {
    std::vector<std::string> cols;
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> tokens;
    browser_->document()->fill_attribute_rows(&cols, &rows, &tokens);
    attribute_table_->set_columns(cols);
    attribute_table_->set_rows(rows);
    attribute_table_->set_row_tokens(std::move(tokens));
  }
  if (feature_info_) {
    if (const MapScene::Feature* f = browser_->document()->selected_feature()) {
      feature_info_->set_feature_id(MapScene::feature_token(f->id));
      std::vector<std::pair<std::string, std::string>> pairs;
      std::string source_layer;
      for (const MapScene::Layer& layer : browser_->document()->layers()) {
        for (const MapScene::Feature& candidate : layer.features) {
          if (std::memcmp(candidate.id.bytes, f->id.bytes,
                          sizeof(f->id.bytes)) == 0 &&
              candidate.id.len == f->id.len) {
            source_layer = layer.name;
            break;
          }
        }
        if (!source_layer.empty()) {
          break;
        }
      }
      const double map_scale =
          browser_->view_frame() ? browser_->view_frame()->scale() : 8.0;
      browser_->document()->fill_feature_info_fields(*f, &pairs, source_layer,
                                                     map_scale);
      std::vector<ui::views::FeatureInfo::Field> fields;
      fields.reserve(pairs.size());
      for (auto& p : pairs) {
        fields.push_back({std::move(p.first), std::move(p.second)});
      }
      feature_info_->set_fields(fields);
    }
  }
}

void BrowserView::wire_edit_feedback() {
  if (!browser_->edit_host() || !browser_->edit_host()->events()) {
    return;
  }
  *browser_->selection_sub() = browser_->edit_host()->events()->subscribe<content::SelectionChanged>(
      [this](const content::SelectionChanged& ev) {
        if (ev.ids.empty()) {
          // Prefer MapScene hit-test result from draft_observer; only clear
          // when the workspace explicitly cleared selection.
          if (!browser_->document()->selected_feature()) {
            set_status_message("Selection cleared");
            if (feature_info_) {
              feature_info_->clear();
            }
          }
          return;
        }
        set_status_message("Selected " + std::to_string(ev.ids.size()) +
                           " feature(s)");
      });
  *browser_->edit_sub() = browser_->edit_host()->events()->subscribe<content::EditCommitted>(
      [this](const content::EditCommitted& ev) {
        const char* op = "modify";
        if (ev.op == content::EditCommitted::Op::kAppend) {
          op = "append";
        } else if (ev.op == content::EditCommitted::Op::kDelete) {
          op = "delete";
        }
        set_status_message(std::string("Committed ") + op);
        sync_inspectors_from_scene();
        invalidate_map_overlays();
      });
  *browser_->extent_sub() = browser_->edit_host()->events()->subscribe<content::ExtentChanged>(
      [this](const content::ExtentChanged& ev) {
        browser_->OnExtentChanged(ev.view_id, ev.extent);
      });

  if (attribute_table_) {
    attribute_table_->set_on_cell_commit(
        [this](const std::string& feature_token, const std::string& field,
               const std::string& value) {
          if (!browser_->document()->update_feature_field(feature_token, field, value)) {
            set_status_message("Attribute edit failed: " + field);
            return false;
          }
          content::ViewHost* host = active_view_host();
          if (host && host->edits()) {
            gis::FeatureMutation mutation;
            mutation.op = gis::EditOp::kModify;
            mutation.id = MapScene::feature_id_from_token(feature_token);
            host->edits()->commit(mutation);
          }
          set_status_message("Updated " + field + "=" + value);
          sync_inspectors_from_scene();
          invalidate_map_overlays();
          return true;
        });
    attribute_table_->set_selected([this](int row) {
      if (!attribute_table_) {
        return;
      }
      const std::string& token = attribute_table_->row_token(row);
      if (token.empty()) {
        return;
      }
      const content::FeatureId id = MapScene::feature_id_from_token(token);
      if (!browser_->document()->select_feature(id)) {
        return;
      }
      sync_inspectors_from_scene();
      invalidate_map_overlays();
      set_status_message("Selected " + token);
    });
  }
}

}  // namespace app
