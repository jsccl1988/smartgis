// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_AMBOX_VIEW_H_
#define UI_VIEWS_AMBOX_VIEW_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/views/view.h"

namespace content {
class PluginHost;
}

namespace tool {
class CommandCatalog;
}

namespace ui {
namespace views {

// Outlook-style toolbox: vertical groups of Label + Button command rows.
// Replaces leftover SmtAMBoxMgrDocBar / SmtXAMBox (CBCGPOutlookBar). Does
// not wrap CView or any MFC bar.
class AmboxView : public View {
 public:
  struct Item {
    std::string id;
    std::string label;
  };

  struct Group {
    std::string name;
    std::vector<Item> items;
  };

  using CommandHandler = std::function<void(const std::string& id)>;

  AmboxView();
  ~AmboxView() override;

  void set_groups(std::vector<Group> groups);
  const std::vector<Group>& groups() const { return groups_; }

  void set_command_handler(CommandHandler handler);

  // content::PluginHost has no public enumerator for contributions, so this
  // installs dummy Select / Pan groups (Identify is a Select item) when
  // listing is unavailable. When |host->commands()| is set, groups are derived
  // from CommandCatalog ids. Includes only content/public from the .cc.
  void populate_from_plugin_host(content::PluginHost* host);

  // Build toolbox groups from a CommandCatalog (testable without PluginHost).
  void populate_from_commands(tool::CommandCatalog* catalog);

  void layout() override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  class GroupBlock;

  void rebuild();
  void fire_command(const std::string& id);

  std::vector<Group> groups_;
  CommandHandler handler_;
  std::vector<GroupBlock*> blocks_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_AMBOX_VIEW_H_
