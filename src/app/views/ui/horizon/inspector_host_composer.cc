// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/inspector_host_composer.h"

#include "app/views/ui/browser_view.h"
#include "app/views/ui/panels/plugin_catalog_view.h"
#include "app/views/ui/panels/report_panel.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/browser/browser.h"
#include "content/browser/session/browser_session.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/atmosphere/panel.h"
#include "plugin/runtime/host/capability/capability.h"
#include "ui/gis/analysis/processing_panel.h"
#include "ui/gis/analysis/result_playback_panel.h"
#include "ui/gis/analysis/spatial_analysis_panel.h"
#include "ui/gis/debug/diagnostic_tools_panel.h"
#include "ui/gis/inspect/attribute_table.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/inspect/measure_panel.h"
#include "ui/gis/inspect/selection_panel.h"
#include "ui/gis/shell/atmosphere_panel.h"
#include "ui/gis/style/layer_properties_panel.h"
#include "ui/gis/style/legend_panel.h"
#include "ui/gis/shell/ambox_view.h"
#include "ui/views/kernel/view/view.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {

InspectorHostComposer::InspectorHostComposer(BrowserView* host) : host_(host) {}

void InspectorHostComposer::ensure_inspector_tab(int index) {
  if (!host_->inspector_tabs_ || index < 0) {
    return;
  }
  if (index == host_->feature_info_tab_ && !host_->feature_info_) {
    auto panel = std::make_unique<ui::views::FeatureInfo>();
    host_->feature_info_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_edit_feedback();
  } else if (index == host_->feature_info_tab_ + 1 && host_->feature_info_tab_ >= 0 &&
             !host_->attribute_table_) {
    auto panel = std::make_unique<ui::views::AttributeTable>();
    host_->attribute_table_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_edit_feedback();
  } else if (index == host_->measure_tab_ && !host_->measure_panel_) {
    auto panel = std::make_unique<ui::views::MeasurePanel>();
    host_->measure_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_measure_panel();
  } else if (index == host_->selection_tab_ && !host_->selection_panel_) {
    auto panel = std::make_unique<ui::views::SelectionPanel>();
    host_->selection_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_selection_panel();
  } else if (index == host_->layer_props_tab_ && !host_->layer_properties_panel_) {
    auto panel = std::make_unique<ui::views::LayerPropertiesPanel>();
    host_->layer_properties_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_layer_properties_panel();
  } else if (index == host_->legend_tab_ && !host_->legend_panel_) {
    auto panel = std::make_unique<ui::views::LegendPanel>();
    host_->legend_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_legend_panel();
  } else if (index == host_->spatial_analysis_tab_ && !host_->spatial_analysis_panel_) {
    auto panel = std::make_unique<ui::views::SpatialAnalysisPanel>();
    host_->spatial_analysis_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_spatial_analysis_panel();
  } else if (index == host_->processing_tab_ && !host_->processing_panel_) {
    auto panel = std::make_unique<ui::views::ProcessingPanel>();
    host_->processing_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_processing_panel();
  } else if (index == host_->playback_tab_ && !host_->result_playback_panel_) {
    auto panel = std::make_unique<ui::views::ResultPlaybackPanel>();
    host_->result_playback_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    host_->wire_result_playback_panel();
  } else if (index == host_->report_tab_ && !host_->report_panel_) {
    auto panel = std::make_unique<ReportPanel>();
    host_->report_panel_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    // P1-3: WebView2 ReportBrowser is created inside wire_report_panel.
    host_->wire_report_panel();
  } else if (index == host_->atmosphere_tab_ && !host_->atmosphere_panel_) {
    // The dock factory lives in the world3d DLL and often never mounts into
    // this tab, leaving the placeholder. Build the panel in the shell.
    auto panel = std::make_unique<ui::views::AtmospherePanel>();
    host_->atmosphere_panel_ = panel.get();
    if (host_->browser_ && host_->browser_->plugins()) {
      (void)host_->browser_->plugins()->ensure_builtins();
      if (content::PluginHost* plugin_host = host_->browser_->plugins()->host()) {
        plugin::detail::wire_atmosphere_panel(panel.get(), plugin_host);
      }
    }
    host_->inspector_tabs_->replace_page(index, std::move(panel));
    // wire_atmosphere_panel forces the boxes off and does not notify. Mirror
    // the live session so Ocean/Cloud/Sky/Fog/Wind match the 3D draw.
    if (host_->browser_ && host_->atmosphere_panel_) {
      bool ocean = false;
      bool cloud = false;
      bool sky = false;
      bool fog = false;
      bool wind = false;
      host_->browser_->session().scene3d_atmosphere_flags(
          &ocean, &cloud, &sky, &fog, &wind);
      host_->atmosphere_panel_->set_ocean_checked(ocean);
      host_->atmosphere_panel_->set_cloud_checked(cloud);
      host_->atmosphere_panel_->set_sky_checked(sky);
      host_->atmosphere_panel_->set_fog_checked(fog);
      host_->atmosphere_panel_->set_wind_checked(wind);
      host_->atmosphere_panel_->set_time_sec(
          host_->browser_->session().scene3d_time_sec());
    }
  } else if (index == host_->plugins_tab_ && !host_->plugin_catalog_) {
    PluginShell* shell = host_->browser_->plugins();
    if (!shell) {
      return;
    }
    (void)shell->ensure_builtins();
    if (!shell->registry() || !shell->host()) {
      return;
    }
    auto panel = std::make_unique<PluginCatalogView>(
        shell->registry(), shell->host(), host_->browser_);
    host_->plugin_catalog_ = panel.get();
    host_->inspector_tabs_->replace_page(index, std::move(panel));
  }
}


void InspectorHostComposer::show_inspector_tab_index(int index) {
  if (host_->inspector_tabs_ && index >= 0) {
    const InspectorGroupBar::Group group = group_for_tab(index);
    if (group != group_ || !host_->inspector_tabs_->tab_included(index)) {
      apply_group(group, index, false);
    }
    note_tab_activated(index);
  }
  host_->ensure_inspector_tab(index);
  if (host_->inspector_tabs_ && index >= 0) {
    host_->inspector_tabs_->set_active(index);
  }
}


void InspectorHostComposer::activate_inspector_tab(int index) {
  host_->show_inspector_tab_index(index);
}


void InspectorHostComposer::ensure_processing_panel() {
  host_->ensure_inspector_tab(host_->processing_tab_);
}


void InspectorHostComposer::on_processing() {
  // Prefer Analysis; fall back to Processing — both may be lazy.
  const int prefer =
      host_->spatial_analysis_tab_ >= 0 ? host_->spatial_analysis_tab_ : host_->processing_tab_;
  host_->ensure_inspector_tab(prefer);
  host_->ensure_inspector_tab(host_->processing_tab_);
  host_->show_inspector_tab_index(prefer);
  if (host_->spatial_analysis_panel_ &&
      !host_->spatial_analysis_panel_->selected_id().empty()) {
    host_->set_status_message(std::string("Analysis: ") +
                       host_->spatial_analysis_panel_->selected_id());
  } else if (host_->processing_panel_ && !host_->processing_panel_->selected_id().empty()) {
    host_->set_status_message(std::string("Processing: ") +
                       host_->processing_panel_->selected_id());
  } else {
    host_->set_status_message("Spatial analysis toolbox");
  }
}


void InspectorHostComposer::show_feature_info_tab() {
  host_->show_inspector_tab_index(host_->feature_info_tab_ >= 0 ? host_->feature_info_tab_ : 0);
}


void InspectorHostComposer::attach_plugin_shell_ui() {
  if (!host_->browser_ || !host_->browser_->plugins() || !host_->browser_->plugins()->host()) {
    return;
  }
  plugin::ShellUiSink* ui = plugin::shell_ui(host_->browser_->plugins()->host());
  if (!ui) {
    return;
  }
  ui->set_bridges(
      [this](std::string_view dock_id, std::string_view title,
             std::unique_ptr<ui::views::View> page) {
        if (!page || !host_->inspector_tabs_) {
          return false;
        }
        std::function<ui::views::AtmospherePanel*(ui::views::View*)> find_atmo =
            [&](ui::views::View* v) -> ui::views::AtmospherePanel* {
          if (!v) {
            return nullptr;
          }
          if (auto* p = dynamic_cast<ui::views::AtmospherePanel*>(v)) {
            return p;
          }
          for (size_t i = 0; i < v->child_count(); ++i) {
            if (auto* p = find_atmo(v->child_at(i))) {
              return p;
            }
          }
          return nullptr;
        };
        if (dock_id == "world3d.atmosphere" && host_->atmosphere_tab_ >= 0) {
          host_->atmosphere_panel_ = find_atmo(page.get());
          return host_->inspector_tabs_->replace_page(host_->atmosphere_tab_,
                                               std::move(page));
        }
        (void)title;
        host_->inspector_tabs_->add_tab(std::string(title), std::move(page));
        sync_included_mask();
        return true;
      },
      [this](int tab) {
        if (!host_->diagnostic_tools_) {
          return false;
        }
        host_->diagnostic_tools_->set_visible_tools(true);
        host_->diagnostic_tools_->set_active_tab(tab);
        return true;
      },
      [this](std::string_view panel) {
        if (panel == "measure") {
          host_->show_inspector_tab_index(host_->measure_tab_);
          return true;
        }
        if (panel == "selection") {
          host_->show_inspector_tab_index(host_->selection_tab_);
          return true;
        }
        if (panel == "legend") {
          host_->show_inspector_tab_index(host_->legend_tab_);
          return true;
        }
        if (panel == "layer") {
          host_->show_inspector_tab_index(host_->layer_props_tab_);
          return true;
        }
        if (panel == "atmosphere") {
          host_->show_inspector_tab_index(host_->atmosphere_tab_);
          return true;
        }
        if (panel == "plugins") {
          host_->show_inspector_tab_index(host_->plugins_tab_);
          return true;
        }
        return false;
      });
}


void InspectorHostComposer::bind_group_bar(InspectorGroupBar* bar) {
  group_bar_ = bar;
  if (group_bar_) {
    group_bar_->set_selected(group_);
  }
}

void InspectorHostComposer::show_inspector_group(InspectorGroupBar::Group group,
                                                 bool materialize) {
  const int slot = static_cast<int>(group);
  const int prefer =
      slot >= 0 && slot < static_cast<int>(InspectorGroupBar::Group::kCount)
          ? last_tab_[slot]
          : -1;
  apply_group(group, prefer, materialize);
}

void InspectorHostComposer::note_tab_activated(int index) {
  if (index < 0) {
    return;
  }
  const int slot = static_cast<int>(group_for_tab(index));
  if (slot >= 0 && slot < static_cast<int>(InspectorGroupBar::Group::kCount)) {
    last_tab_[slot] = index;
  }
}

void InspectorHostComposer::sync_included_mask() {
  ui::views::TabStrip* tabs = host_->inspector_tabs_;
  if (!tabs) {
    return;
  }
  for (int i = 0; i < tabs->tab_count(); ++i) {
    tabs->set_tab_included(i, group_for_tab(i) == group_);
  }
}

void InspectorHostComposer::set_group_bar_extent(int width_px, int height_px) {
  if (!group_bar_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  group_bar_->set_preferred_size({width_px, height_px});
}

InspectorGroupBar::Group InspectorHostComposer::group_for_tab(int index) const {
  if (host_->inspector_tabs_ && host_->side_ambox_ &&
      host_->inspector_tabs_->page_at(index) == host_->side_ambox_) {
    return InspectorGroupBar::Group::kMap;
  }
  if (host_->feature_info_tab_ >= 0 &&
      (index == host_->feature_info_tab_ ||
       index == host_->feature_info_tab_ + 1)) {
    return InspectorGroupBar::Group::kInspect;
  }
  if (index == host_->measure_tab_ || index == host_->selection_tab_) {
    return InspectorGroupBar::Group::kInspect;
  }
  if (index == host_->layer_props_tab_ || index == host_->legend_tab_) {
    return InspectorGroupBar::Group::kMap;
  }
  if (index == host_->spatial_analysis_tab_ || index == host_->processing_tab_ ||
      index == host_->playback_tab_ || index == host_->report_tab_) {
    return InspectorGroupBar::Group::kProcess;
  }
  return InspectorGroupBar::Group::kExtend;
}

void InspectorHostComposer::apply_group(InspectorGroupBar::Group group,
                                       int prefer, bool materialize) {
  group_ = group;
  if (group_bar_) {
    group_bar_->set_selected(group);
  }
  sync_included_mask();
  ui::views::TabStrip* tabs = host_->inspector_tabs_;
  if (!tabs) {
    return;
  }
  int show = prefer;
  if (show < 0 || group_for_tab(show) != group) {
    show = -1;
    for (int i = 0; i < tabs->tab_count(); ++i) {
      if (group_for_tab(i) == group) {
        show = i;
        break;
      }
    }
  }
  if (show < 0) {
    return;
  }
  note_tab_activated(show);
  if (tabs->active() != show) {
    tabs->set_active(show);
  }
  // Plugins stays deferred (materialize=false) so first-map does not scan the
  // catalog. Atmosphere is a local markup panel — fill it whenever it is the
  // page on screen, otherwise the dock stays a blank placeholder.
  if (materialize || show == host_->atmosphere_tab_) {
    host_->ensure_inspector_tab(show);
  }
}

}  // namespace app
