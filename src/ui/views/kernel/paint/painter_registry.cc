// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/paint/painter_registry.h"

#include <utility>

namespace ui {
namespace views {

PainterRegistry& PainterRegistry::get() {
  static PainterRegistry* instance = new PainterRegistry();
  return *instance;
}

void PainterRegistry::register_painter(std::string_view role,
                                       std::unique_ptr<Painter> painter) {
  if (role.empty() || !painter) {
    return;
  }
  const std::string key(role);
  auto& slot = entries_[key];
  if (!slot) {
    slot = std::make_unique<Entry>();
  }
  Entry& e = *slot;
  e.painter = std::move(painter);
  e.plugin_id.clear();
  e.previous.reset();
  e.previous_plugin_id.clear();
}

void PainterRegistry::register_painter_for_plugin(
    std::string_view plugin_id,
    std::string_view role,
    std::unique_ptr<Painter> painter) {
  if (plugin_id.empty() || role.empty() || !painter) {
    return;
  }
  const std::string key(role);
  auto& slot = entries_[key];
  if (!slot) {
    slot = std::make_unique<Entry>();
  }
  Entry& e = *slot;
  e.previous = std::move(e.painter);
  e.previous_plugin_id = std::move(e.plugin_id);
  e.painter = std::move(painter);
  e.plugin_id = std::string(plugin_id);
}

Painter* PainterRegistry::find(std::string_view role) const {
  // Reject empty and absurd lengths. A cross-DLL View vtable mismatch can
  // return a garbage string_view; constructing std::string would throw
  // length_error and abort the process (MSVC exit 3).
  constexpr size_t kMaxRoleLen = 128;
  if (role.empty() || role.size() > kMaxRoleLen) {
    return nullptr;
  }
  const auto it = entries_.find(std::string(role));
  if (it == entries_.end() || !it->second) {
    return nullptr;
  }
  return it->second->painter.get();
}

void PainterRegistry::withdraw_plugin(std::string_view plugin_id) {
  if (plugin_id.empty()) {
    return;
  }
  const std::string pid(plugin_id);
  std::vector<std::string> roles;
  for (const auto& [role, slot] : entries_) {
    if (slot && slot->plugin_id == pid) {
      roles.push_back(role);
    }
  }
  for (const std::string& role : roles) {
    auto it = entries_.find(role);
    if (it == entries_.end() || !it->second) {
      continue;
    }
    Entry& e = *it->second;
    e.painter = std::move(e.previous);
    e.plugin_id = std::move(e.previous_plugin_id);
    e.previous.reset();
    e.previous_plugin_id.clear();
    if (!e.painter) {
      entries_.erase(it);
    }
  }
}

}  // namespace views
}  // namespace ui
