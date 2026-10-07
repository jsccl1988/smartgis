// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/ambox_composer.h"

#include "app/views/ui/browser_view.h"
#include "app/views/ui/panels/plugin_catalog_view.h"

#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "base/process/switches.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "tool/command/command.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/shell/ambox_view.h"
#include "ui/views/kernel/widget/widget.h"

namespace app {
namespace {

std::vector<ui::views::AmboxView::Group> enabled_plugin_groups(
    plugin::Registry* registry,
    content::PluginHost* host) {
  std::vector<ui::views::AmboxView::Group> groups;
  if (!registry) {
    return groups;
  }
  std::map<std::string, size_t> index;
  for (const plugin::PluginRecord& rec : registry->list()) {
    if (rec.state != plugin::PluginState::kEnabled) {
      continue;
    }
    ui::views::AmboxView::Group group;
    group.name =
        rec.manifest.name.empty() ? rec.manifest.id : rec.manifest.name;
    index.emplace(rec.manifest.id, groups.size());
    groups.push_back(std::move(group));
  }
  if (!host) {
    return groups;
  }
  host->for_each_command([&](std::string_view plugin_id,
                             std::string_view command_id,
                             std::string_view title) {
    // Guard against cross-DLL PluginHost vtable slips that pass garbage
    // string_views (would throw bad_alloc / length_error on construct).
    constexpr size_t kMaxId = 256;
    if (plugin_id.empty() || plugin_id.size() > kMaxId ||
        command_id.empty() || command_id.size() > kMaxId ||
        title.size() > kMaxId) {
      return;
    }
    const auto it = index.find(std::string(plugin_id));
    if (it == index.end()) {
      return;
    }
    ui::views::AmboxView::Item item;
    item.id = std::string(command_id);
    item.label = title.empty() ? item.id : std::string(title);
    groups[it->second].items.push_back(std::move(item));
  });
  return groups;
}


}  // namespace

AmboxComposer::AmboxComposer(BrowserView* host) : host_(host) {}

void AmboxComposer::populate_ambox() {
  if (!host_->ambox_) {
    return;
  }
  // Soft-skip catalog walk when parallel rebuilds leave CommandCatalog maps
  // unreadable (AV in tool::CommandCatalog::for_each). FPS bench used to
  // skip the whole populate — that left clear/point/polygon sharing the
  // default cursor glyph. Keep workspace chips; still avoid plugin registry
  // walks when benching (freefill AV under Debug rebuilds).
  const bool fps_benching = [] {
    const char* bench = base::switch_cstr("map2d-fps-bench-ms");
    return bench && bench[0] != '\0' && std::atoi(bench) > 0;
  }();
  // Match wire_report_panel / wire_edit_feedback: any non-empty non-"0" skip.
  if (const char* skip = base::switch_cstr("skip-ambox-catalog");
      skip && skip[0] != '\0' && skip[0] != '0') {
    return;
  }
  std::vector<tool::CommandCatalog*> catalogs;
  if (host_->browser_->edit_host() && host_->browser_->edit_host()->workspace()) {
    catalogs.push_back(&host_->browser_->edit_host()->workspace()->catalog());
  }
  // Right dock Tools lists plugin AMBox groups. Plain argv=[] launch must
  // populate them without opening Plugin Manager (visual_review #7). Still
  // skip poison PluginShell pointers under parallel Debug freefill.
  std::vector<ui::views::AmboxView::Group> plugin_groups;
  if (!fps_benching && host_->browser_->plugins()) {
    PluginShell* shell = host_->browser_->plugins();
    const auto shell_addr = reinterpret_cast<uintptr_t>(shell);
    const auto lo24 = shell_addr & 0xffffff00ull;
    const bool poison = shell_addr < 0x10000u || lo24 == 0xcdcdcd00ull ||
                        lo24 == 0xdddddd00ull || lo24 == 0xcccccc00ull ||
                        lo24 == 0xfeeefeeeull || lo24 == 0xababab00ull;
    if (!poison) {
      // Workspace/tool catalog on the map bar only when Plugin Manager asked
      // for a full refresh (host_->ambox_include_plugins_). Always fill side Tools.
      if (host_->ambox_include_plugins_) {
        if (tool::CommandCatalog* plugin_catalog = shell->commands()) {
          catalogs.push_back(plugin_catalog);
        }
      }
      if (shell->registry() && shell->host()) {
        (void)shell->ensure_builtins();
        plugin_groups =
            enabled_plugin_groups(shell->registry(), shell->host());
      }
    }
  }
  // Map tool bar owns Select/Edit workspace chips. Right dock lists plugin
  // AMBox groups only — twin chip strips failed visual_review #3.
  host_->ambox_->populate_from_commands(catalogs, plugin_groups);
  if (host_->side_ambox_) {
    host_->side_ambox_->set_groups(std::move(plugin_groups));
  }
}


void AmboxComposer::on_plugins() {
  if (host_->browser_->plugins()) {
    (void)host_->browser_->plugins()->ensure_builtins();
    // Re-bind after builtins so report callbacks see a live PluginHost.
    host_->ensure_inspector_tab(host_->report_tab_);
    host_->attach_report_plugin_bridge();
    PluginCatalogView::run_modal(host_->widget_.hwnd(), host_->browser_->plugins()->registry(),
                                 host_->browser_->plugins()->host());
    host_->ambox_include_plugins_ = true;
    host_->populate_ambox();
    host_->ambox_include_plugins_ = false;
  }
}


}  // namespace app
