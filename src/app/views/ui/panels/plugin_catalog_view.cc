// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/plugin_catalog_view.h"

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "app/views/app/startup/scenario.h"
#include "app/views/browser/browser.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "ui/views/dialogs/dialog.h"
#include "ui/views/kernel/layout/layout.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/primitives/text/label.h"

namespace app {
namespace {

const char* kind_name(plugin::PluginKind k) {
  switch (k) {
    case plugin::PluginKind::kBuiltin:
      return "builtin";
    case plugin::PluginKind::kNative:
      return "native";
    case plugin::PluginKind::kPython:
      return "python";
    case plugin::PluginKind::kLegacyAm:
      return "legacy_am";
  }
  return "?";
}

const char* state_name(plugin::PluginState s) {
  switch (s) {
    case plugin::PluginState::kDisabled:
      return "disabled";
    case plugin::PluginState::kEnabled:
      return "enabled";
    case plugin::PluginState::kError:
      return "error";
    case plugin::PluginState::kInvalidExports:
      return "invalid_exports";
    case plugin::PluginState::kLoadFailed:
      return "load_failed";
  }
  return "?";
}

const char* trust_name(plugin::TrustClass t) {
  switch (t) {
    case plugin::TrustClass::kBuiltin:
      return "builtin";
    case plugin::TrustClass::kSignedOfficial:
      return "signed";
    case plugin::TrustClass::kUnsignedTrusted:
      return "trusted";
    case plugin::TrustClass::kDenied:
      return "denied";
  }
  return "?";
}

}  // namespace

PluginCatalogView::PluginCatalogView(plugin::Registry* registry,
                                     content::PluginHost* host,
                                     Browser* browser)
    : registry_(registry), host_(host), browser_(browser) {
  auto layout = std::make_unique<ui::views::BoxLayout>(
      ui::views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(4);
  ui::views::BoxLayout* box = layout.get();
  set_layout_manager(std::move(layout));

  auto table = std::make_unique<ui::views::TableView>();
  table_ = table.get();
  // Narrow right-dock columns; kind/version/path live in the hover tip.
  table_->set_columns({"id", "name", "state"});
  table_->set_row_hover([this](int row) { on_hover_row(row); });
  table_->set_preferred_size({280, 0});
  box->set_flex_for_view(table_, 1);
  add_child(std::move(table));

  auto tip = std::make_unique<ui::views::Label>("Hover a plugin for details");
  tooltip_ = tip.get();
  add_child(std::move(tip));

  auto enable = std::make_unique<ui::views::Button>("Enable");
  enable->set_click([this]() { on_enable(); });
  add_child(std::move(enable));

  auto disable = std::make_unique<ui::views::Button>("Disable");
  disable->set_click([this]() { on_disable(); });
  add_child(std::move(disable));

  auto run = std::make_unique<ui::views::Button>("Run");
  run->set_click([this]() { on_run(); });
  add_child(std::move(run));

  auto err = std::make_unique<ui::views::Label>("");
  error_ = err.get();
  add_child(std::move(err));

  // Match other right-dock inspector pages (Processing / Report).
  set_preferred_size({280, 200});
  refresh();
}

bool PluginCatalogView::run_modal(HWND owner, plugin::Registry* registry,
                                  content::PluginHost* host,
                                  Browser* browser) {
  if (!registry || !host) {
    return false;
  }
  ui::views::Dialog::run_modal(
      owner, L"Plugins", 720, 420,
      std::make_unique<PluginCatalogView>(registry, host, browser));
  return true;
}

void PluginCatalogView::refresh() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  if (!registry_) {
    return;
  }
  for (const plugin::PluginRecord& rec : registry_->list()) {
    table_->add_row({rec.manifest.id, rec.manifest.name, state_name(rec.state)});
  }
  if (error_) {
    error_->set_text(registry_->last_error());
  }
}

void PluginCatalogView::on_hover_row(int row) {
  if (!tooltip_) {
    return;
  }
  if (row < 0) {
    tooltip_->set_text("Hover a plugin for details");
    return;
  }
  tooltip_->set_text(tooltip_for_row(row));
}

std::string PluginCatalogView::tooltip_for_row(int row) const {
  if (!registry_ || row < 0) {
    return {};
  }
  const std::vector<plugin::PluginRecord> rows = registry_->list();
  if (row >= static_cast<int>(rows.size())) {
    return {};
  }
  const plugin::PluginRecord& rec = rows[static_cast<size_t>(row)];
  std::string tip = rec.manifest.name.empty() ? rec.manifest.id : rec.manifest.name;
  tip += "  v";
  tip += rec.manifest.version.empty() ? "?" : rec.manifest.version;
  tip += "  ";
  tip += kind_name(rec.manifest.kind);
  tip += "/";
  tip += state_name(rec.state);
  tip += "/";
  tip += trust_name(rec.trust);
  if (!rec.directory.empty()) {
    tip += "  path=";
    tip += rec.directory;
  }
  if (!rec.manifest.description.empty()) {
    tip += "  ";
    tip += rec.manifest.description;
  }
  const std::string sid = scenario_id_for_plugin(rec.manifest.id);
  if (!sid.empty()) {
    tip += "  scenario=";
    tip += sid;
  }
  return tip;
}

std::string PluginCatalogView::selected_id() const {
  if (!table_ || table_->selected_row() < 0) {
    return {};
  }
  const int row = table_->selected_row();
  if (row >= static_cast<int>(table_->row_count())) {
    return {};
  }
  const std::vector<std::string>& cells = table_->row_at(static_cast<size_t>(row));
  return cells.empty() ? std::string() : cells[0];
}

std::string PluginCatalogView::scenario_id_for_plugin(
    const std::string& plugin_id) const {
  if (plugin_id.empty()) {
    return {};
  }
  ensure_builtin_scenarios();
  // Already a registry id (plugin.flood / browser.map2d.china).
  if (find_scenario(plugin_id)) {
    return plugin_id;
  }
  // Catalog package ids map to plugin.<id> rows.
  const std::string qualified = "plugin." + plugin_id;
  if (find_scenario(qualified)) {
    return qualified;
  }
  // map2d package demos: default china carto showcase.
  if (plugin_id == "map2d" && find_scenario("browser.map2d.china")) {
    return "browser.map2d.china";
  }
  return {};
}

void PluginCatalogView::on_enable() {
  if (!registry_ || !host_) {
    return;
  }
  const std::string id = selected_id();
  if (id.empty()) {
    return;
  }
  // #region agent log
  {
    FILE* f = nullptr;
    if (fopen_s(&f, "C:/Dev/src/gis/smartgis/debug-778a24.log", "a") == 0 &&
        f) {
      std::fprintf(
          f,
          "{\"sessionId\":\"778a24\",\"runId\":\"pre\",\"hypothesisId\":\"F\","
          "\"location\":\"plugin_catalog_view.cc:on_enable\",\"message\":"
          "\"catalog enable\",\"data\":{\"id\":\"%s\"},\"timestamp\":0}\n",
          id.c_str());
      std::fclose(f);
    }
  }
  // #endregion
  (void)registry_->set_enabled(id, true, host_);
  // #region agent log
  {
    FILE* f = nullptr;
    if (fopen_s(&f, "C:/Dev/src/gis/smartgis/debug-778a24.log", "a") == 0 &&
        f) {
      std::fprintf(
          f,
          "{\"sessionId\":\"778a24\",\"runId\":\"pre\",\"hypothesisId\":\"F\","
          "\"location\":\"plugin_catalog_view.cc:on_enable_done\",\"message\":"
          "\"catalog enable done\",\"data\":{\"id\":\"%s\"},\"timestamp\":0}\n",
          id.c_str());
      std::fclose(f);
    }
  }
  // #endregion
  refresh();
}

void PluginCatalogView::on_disable() {
  if (!registry_ || !host_) {
    return;
  }
  const std::string id = selected_id();
  if (id.empty()) {
    return;
  }
  (void)registry_->set_enabled(id, false, host_);
  refresh();
}

void PluginCatalogView::on_run() {
  if (!browser_) {
    if (error_) {
      error_->set_text("Run needs Browser (docked panel)");
    }
    return;
  }
  const std::string plugin_id = selected_id();
  if (plugin_id.empty()) {
    if (error_) {
      error_->set_text("Select a plugin first");
    }
    return;
  }
  // #region agent log
  {
    FILE* f = nullptr;
    if (fopen_s(&f, "C:/Dev/src/gis/smartgis/debug-778a24.log", "a") == 0 &&
        f) {
      std::fprintf(
          f,
          "{\"sessionId\":\"778a24\",\"runId\":\"pre\",\"hypothesisId\":\"F\","
          "\"location\":\"plugin_catalog_view.cc:on_run\",\"message\":\"catalog "
          "run\",\"data\":{\"id\":\"%s\"},\"timestamp\":0}\n",
          plugin_id.c_str());
      std::fclose(f);
    }
  }
  // #endregion
  const std::string sid = scenario_id_for_plugin(plugin_id);
  if (sid.empty()) {
    if (error_) {
      error_->set_text("No scenario for " + plugin_id);
    }
    return;
  }
  const Scenario* scenario = find_scenario(sid);
  if (!scenario) {
    if (error_) {
      error_->set_text("Unknown scenario " + sid);
    }
    return;
  }
  if (error_) {
    error_->set_text("Running " + sid + "…");
  }
  const int rc = run_scenario(*browser_, *scenario);
  if (error_) {
    if (rc == 0) {
      error_->set_text("Run OK: " + sid);
    } else {
      error_->set_text("Run failed (" + std::to_string(rc) + "): " + sid);
    }
  }
}

}  // namespace app
