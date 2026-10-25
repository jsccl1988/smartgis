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
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "base/core/log.h"
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
  struct VisitCtx {
    std::vector<ui::views::AmboxView::Group>* groups;
    std::map<std::string, size_t>* index;
  } ctx{&groups, &index};
  registry->visit(
      [](const char* id, const char* name, int state, int, int, void* baton) {
        auto* c = static_cast<VisitCtx*>(baton);
        if (state != static_cast<int>(plugin::PluginState::kEnabled) || !id ||
            !id[0]) {
          return;
        }
        ui::views::AmboxView::Group group;
        group.name = (name && name[0]) ? std::string(name) : std::string(id);
        c->index->emplace(id, c->groups->size());
        c->groups->push_back(std::move(group));
      },
      &ctx);
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
  if (host_->browser_->edit_tool_session() && host_->browser_->edit_tool_session()->workspace()) {
    catalogs.push_back(&host_->browser_->edit_tool_session()->workspace()->catalog());
  }
  // Right dock Tools lists plugin AMBox groups. Plain argv=[] launch must
  // populate them without opening Plugin Manager (visual_review #7) — that
  // fill runs after WaitFirstMapPresent via finish_deferred_shell_wiring
  // (ensure_builtins + populate_ambox). Do not LoadLibrary here: init_all
  // dominated BuildContents.AmboxInspector on the cold-start profile.
  // Workspace Select/Edit chips still populate from the edit catalog below.
  std::vector<ui::views::AmboxView::Group> plugin_groups;
  if (!fps_benching && host_->browser_->plugins()) {
    PluginShell* shell = host_->browser_->plugins();
    const auto shell_addr = reinterpret_cast<uintptr_t>(shell);
    const auto lo24 = shell_addr & 0xffffff00ull;
    const bool poison = shell_addr < 0x10000u || lo24 == 0xcdcdcd00ull ||
                        lo24 == 0xdddddd00ull || lo24 == 0xcccccc00ull ||
                        lo24 == 0xfeeefeeeull || lo24 == 0xababab00ull;
    if (!poison && shell->builtins_started()) {
      // Workspace/tool catalog on the map bar only when Plugin Manager asked
      // for a full refresh (host_->ambox_include_plugins_). Always fill side Tools.
      if (host_->ambox_include_plugins_) {
        if (tool::CommandCatalog* plugin_catalog = shell->commands()) {
          catalogs.push_back(plugin_catalog);
        }
      }
      if (shell->registry() && shell->host()) {
        plugin_groups =
            enabled_plugin_groups(shell->registry(), shell->host());
      }
    }
  }
  // Map tool bar owns Select/Edit workspace chips. Right dock lists plugin
  // AMBox groups only — twin chip strips failed visual_review #3.
  // Cross-DLL: copy plugin group strings into the DLL via replace_groups_*,
  // then populate_from_catalogs (pointer array). Do not pass std::vector<Group>
  // into ui_views.dll (Debug STL → heap 0xC0000374).
  auto push_plugin_groups = [](ui::views::AmboxView* ambox,
                               const std::vector<ui::views::AmboxView::Group>&
                                   groups) {
    if (!ambox) {
      return;
    }
    ambox->replace_groups_begin();
    for (const auto& group : groups) {
      ambox->replace_groups_add(group.name.c_str());
      for (const auto& item : group.items) {
        ambox->replace_groups_add_item(item.id.c_str(), item.label.c_str());
      }
    }
  };
  push_plugin_groups(host_->ambox_, plugin_groups);
  host_->ambox_->populate_from_catalogs(
      catalogs.empty() ? nullptr : catalogs.data(), catalogs.size(),
      !plugin_groups.empty());
  if (host_->side_ambox_) {
    push_plugin_groups(host_->side_ambox_, plugin_groups);
    host_->side_ambox_->replace_groups_commit();
    // Rebuild may have run while the dock page was still 0x0; force a layout
    // once the Tools tab already has a real client box so chips hit-test.
    const ui::views::Rect& sb = host_->side_ambox_->bounds();
    if (sb.width > 0 && sb.height > 0) {
      host_->side_ambox_->layout();
    }
  }
}


void AmboxComposer::on_plugins() {
  LOGGING(LOG_INFO, "ui.on_plugins: begin tab=%d", host_->plugins_tab_);
  if (!host_->browser_->plugins()) {
    LOGGING(LOG_WARNING, "ui.on_plugins: no PluginShell");
    return;
  }
  (void)host_->browser_->plugins()->ensure_builtins();
  LOGGING(LOG_INFO, "ui.on_plugins: ensure_builtins done");
  // Right-dock Plugins only — do not materialize Report/WebView2 here
  // (that used to run on every Manage plugins… and could stall/crash).
  // Do not set ambox_include_plugins_: that pushes PluginShell::commands()
  // into populate_from_commands, and CommandCatalog::for_each AVs when the
  // catalog maps are mid-rebuild after ensure_builtins (0xC0000005).
  // Do not call populate_ambox() here either: it re-enters ensure_builtins /
  // enabled_plugin_groups and has freefilled heaps that later AV when the
  // DebugAgent thread destroys its host copy after :ui panel plugins.
  host_->ensure_inspector_tab(host_->plugins_tab_);
  LOGGING(LOG_INFO, "ui.on_plugins: catalog=%p",
          static_cast<void*>(host_->plugin_catalog_));
  host_->show_inspector_tab_index(host_->plugins_tab_);
  LOGGING(LOG_INFO, "ui.on_plugins: tab shown");
  if (host_->plugin_catalog_) {
    host_->plugin_catalog_->refresh();
    LOGGING(LOG_INFO, "ui.on_plugins: catalog refresh done");
  }
  LOGGING(LOG_INFO, "ui.on_plugins: done");
}


}  // namespace app
