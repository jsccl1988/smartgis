// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/panels/plugin_catalog_view.h"

#include <memory>
#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "ui/views/dialogs/dialog.h"
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
                                     content::PluginHost* host)
    : registry_(registry), host_(host) {
  auto table = std::make_unique<ui::views::TableView>();
  table_ = table.get();
  table_->set_columns({"id", "name", "version", "kind", "state"});
  table_->set_row_hover([this](int row) { on_hover_row(row); });
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

  auto err = std::make_unique<ui::views::Label>("");
  error_ = err.get();
  add_child(std::move(err));

  set_preferred_size({720, 420});
  refresh();
}

bool PluginCatalogView::run_modal(HWND owner, plugin::Registry* registry,
                                  content::PluginHost* host) {
  if (!registry || !host) {
    return false;
  }
  ui::views::Dialog::run_modal(
      owner, L"Plugins", 720, 420,
      std::make_unique<PluginCatalogView>(registry, host));
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
    table_->add_row({rec.manifest.id, rec.manifest.name, rec.manifest.version,
                     kind_name(rec.manifest.kind), state_name(rec.state)});
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

void PluginCatalogView::on_enable() {
  if (!registry_ || !host_) {
    return;
  }
  const std::string id = selected_id();
  if (id.empty()) {
    return;
  }
  (void)registry_->set_enabled(id, true, host_);
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

}  // namespace app
