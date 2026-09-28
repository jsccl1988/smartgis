// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_VIEW_COMMANDS_H_
#define APP_VIEWS_SHELL_VIEW_COMMANDS_H_

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ui/views/primitives/menu/context_menu.h"

namespace app {

// One row of the navigation table shared by the View menu and the map
// right-click. Bookmark jumps are extra rows with id view.bookmark_go.
struct ViewCommandId {
  const char* label;
  const char* id;
};

std::span<const ViewCommandId> navigation_commands();

// Fixed navigation ids, then one view.bookmark_go per bookmark label.
std::vector<std::string> navigation_command_ids(
    const std::vector<std::string>& bookmark_labels);

// Menu rows for the navigation table. |bookmark_index| is -1 except on
// view.bookmark_go rows.
std::vector<ui::views::MenuItem> navigation_menu_items(
    const std::vector<std::string>& bookmark_labels,
    const std::function<void(std::string_view id, int bookmark_index)>&
        invoke);

// File / Edit / View / Layer rows. View is the navigation table, a separator,
// then Refresh / RHI / MapLibre. One invoke serves every row.
struct ShellMenus {
  std::vector<ui::views::MenuItem> file;
  std::vector<ui::views::MenuItem> edit;
  std::vector<ui::views::MenuItem> view;
  std::vector<ui::views::MenuItem> layer;
};

ShellMenus build_shell_menus(
    const std::vector<std::string>& bookmark_labels,
    const std::function<void(std::string_view id, int bookmark_index)>&
        invoke);

}  // namespace app

#endif  // APP_VIEWS_SHELL_VIEW_COMMANDS_H_
