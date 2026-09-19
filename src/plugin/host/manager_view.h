// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MANAGER_VIEW_H_
#define PLUGIN_MANAGER_VIEW_H_

#include <functional>
#include <string>

#include "ui/views/primitives/label.h"
#include "ui/views/primitives/table_view.h"
#include "ui/views/kernel/view.h"

namespace content {
class PluginHost;
}

namespace plugin {

class Registry;
class Store;

// Plugin Manager table: enable / disable / trust / optional install hooks.
class ManagerView : public ui::views::View {
 public:
  ManagerView(Registry* registry, content::PluginHost* host);
  void refresh();
  size_t plugin_row_count() const;

  void set_install_handler(std::function<bool()> install_zip,
                           std::function<bool()> install_index,
                           std::function<bool()> uninstall);

 private:
  void on_enable();
  void on_disable();
  void on_trust();
  std::string selected_id() const;

  Registry* registry_ = nullptr;
  content::PluginHost* host_ = nullptr;
  ui::views::TableView* table_ = nullptr;
  ui::views::Label* error_ = nullptr;
  std::function<bool()> install_zip_;
  std::function<bool()> install_index_;
  std::function<bool()> uninstall_;
};

}  // namespace plugin

#endif  // PLUGIN_MANAGER_VIEW_H_
