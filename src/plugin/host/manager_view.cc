// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/host/manager_view.h"

#include <memory>

#include "content/public/plugin_host.h"
#include "plugin/host/registry.h"
#include "ui/views/button.h"
#include "ui/views/label.h"
#include "ui/views/table_view.h"

namespace plugin {
namespace {

const char* kind_name(PluginKind k) {
  switch (k) {
    case PluginKind::kBuiltin:
      return "builtin";
    case PluginKind::kNative:
      return "native";
    case PluginKind::kPython:
      return "python";
    case PluginKind::kLegacyAm:
      return "legacy_am";
  }
  return "?";
}

const char* state_name(PluginState s) {
  switch (s) {
    case PluginState::kDisabled:
      return "disabled";
    case PluginState::kEnabled:
      return "enabled";
    case PluginState::kError:
      return "error";
    case PluginState::kInvalidExports:
      return "invalid_exports";
    case PluginState::kLoadFailed:
      return "load_failed";
  }
  return "?";
}

const char* trust_name(TrustClass t) {
  switch (t) {
    case TrustClass::kBuiltin:
      return "builtin";
    case TrustClass::kSignedOfficial:
      return "signed";
    case TrustClass::kUnsignedTrusted:
      return "trusted";
    case TrustClass::kDenied:
      return "denied";
  }
  return "?";
}

}  // namespace

ManagerView::ManagerView(Registry* registry, content::PluginHost* host)
    : registry_(registry), host_(host) {
  auto table = std::make_unique<ui::views::TableView>();
  table_ = table.get();
  table_->set_columns({"id", "name", "version", "kind", "state", "trust"});
  add_child(std::move(table));

  auto enable = std::make_unique<ui::views::Button>("Enable");
  enable->set_click([this]() { on_enable(); });
  add_child(std::move(enable));

  auto disable = std::make_unique<ui::views::Button>("Disable");
  disable->set_click([this]() { on_disable(); });
  add_child(std::move(disable));

  auto trust = std::make_unique<ui::views::Button>("Trust unsigned");
  trust->set_click([this]() { on_trust(); });
  add_child(std::move(trust));

  auto inst = std::make_unique<ui::views::Button>("Install zip");
  inst->set_click([this]() {
    if (install_zip_) {
      install_zip_();
    }
    refresh();
  });
  add_child(std::move(inst));

  auto idx = std::make_unique<ui::views::Button>("Install index");
  idx->set_click([this]() {
    if (install_index_) {
      install_index_();
    }
    refresh();
  });
  add_child(std::move(idx));

  auto un = std::make_unique<ui::views::Button>("Uninstall");
  un->set_click([this]() {
    if (uninstall_) {
      uninstall_();
    }
    refresh();
  });
  add_child(std::move(un));

  auto err = std::make_unique<ui::views::Label>("");
  error_ = err.get();
  add_child(std::move(err));

  set_preferred_size({640, 360});
  refresh();
}

void ManagerView::set_install_handler(std::function<bool()> install_zip,
                                      std::function<bool()> install_index,
                                      std::function<bool()> uninstall) {
  install_zip_ = std::move(install_zip);
  install_index_ = std::move(install_index);
  uninstall_ = std::move(uninstall);
}

void ManagerView::refresh() {
  if (!table_) {
    return;
  }
  table_->clear_rows();
  if (!registry_) {
    return;
  }
  for (const PluginRecord& rec : registry_->list()) {
    table_->add_row({rec.manifest.id, rec.manifest.name, rec.manifest.version,
                     kind_name(rec.manifest.kind), state_name(rec.state),
                     trust_name(rec.trust)});
  }
  if (error_) {
    error_->set_text(registry_->last_error());
  }
}

size_t ManagerView::plugin_row_count() const {
  return table_ ? table_->row_count() : 0;
}

std::string ManagerView::selected_id() const {
  if (!table_ || table_->row_count() == 0) {
    return {};
  }
  const auto& row = table_->row_at(0);
  return row.empty() ? std::string() : row[0];
}

void ManagerView::on_enable() {
  if (!registry_) {
    return;
  }
  registry_->set_enabled(selected_id(), true, host_);
  refresh();
}

void ManagerView::on_disable() {
  if (!registry_) {
    return;
  }
  registry_->set_enabled(selected_id(), false, host_);
  refresh();
}

void ManagerView::on_trust() {
  if (!registry_) {
    return;
  }
  registry_->trust_unsigned(selected_id());
  refresh();
}

}  // namespace plugin
