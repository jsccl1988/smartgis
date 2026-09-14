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

  // When |host| / |host->commands()| is null, installs dummy Select / Pan /
  // Edit groups (Identify is a Select item). Otherwise groups come from
  // CommandCatalog::for_each by id prefix. Includes only content/public
  // from the .cc.
  void populate_from_plugin_host(content::PluginHost* host);

  // Build toolbox groups from one CommandCatalog (testable without PluginHost).
  void populate_from_commands(tool::CommandCatalog* catalog);

  // Merge ids from several catalogs (Workspace + PluginHost) into groups.
  // Null entries are skipped; empty list behaves like a null catalog.
  void populate_from_commands(
      const std::vector<tool::CommandCatalog*>& catalogs);

  void layout() override;

 protected:
  void paint_self(render::skia::Canvas* canvas) override;

 private:
  class GroupBlock;

  void rebuild();
  void fire_command(const std::string& id);
  int measure_content_height(float scale) const;

  std::vector<Group> groups_;
  CommandHandler handler_;
  std::vector<GroupBlock*> blocks_;
  class ScrollView* scroll_ = nullptr;
  View* content_ = nullptr;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_AMBOX_VIEW_H_
