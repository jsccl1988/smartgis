// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_
#define APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/view/view.h"

namespace content {
class PluginHost;
}

namespace plugin {
class Registry;
}

namespace ui {
namespace views {
class BoxLayout;
class Button;
class Label;
class ScrollView;
class TableView;
class Textfield;
}
}  // namespace ui

namespace app {

class Browser;

// Right-dock plugin toolbox. Plugin rows stay on top. The selected plugin's
// commands are a switcher; the active command's form is a vertical stack in
// this panel so the map stays visible.
class PluginCatalogView : public ui::views::View {
 public:
  PluginCatalogView(plugin::Registry* registry, content::PluginHost* host,
                    Browser* browser = nullptr);
  ~PluginCatalogView() override;

  // Optional floating dialog (tests / fallback). Product shell docks this view.
  static bool run_modal(HWND owner, plugin::Registry* registry,
                        content::PluginHost* host, Browser* browser = nullptr);

  void refresh();

 private:
  enum class ToolKind { kCommand, kProcessing, kDialog, kDock };

  struct ToolEntry {
    ToolKind kind = ToolKind::kCommand;
    std::string id;
    std::string title;
  };

  void on_search();
  void on_plugin_click(int row);
  void on_plugin_activate(int row, int col);
  void on_enable();
  void on_disable();
  void on_run();

  void fill_plugins(bool rebuild_tools);
  void fill_tools();
  void fill_detail();
  void show_tool(int index);
  void mount_body();
  void show_run_card(const ToolEntry& tool);
  void clear_form();
  void style_command_buttons();
  void accept_presented(const wchar_t* title,
                        std::unique_ptr<ui::views::View>* body);
  bool plugin_enabled(const std::string& plugin_id) const;
  bool dialog_registered(const std::string& tool_id) const;
  bool processing_registered(const std::string& tool_id) const;
  bool ensure_enabled(const std::string& plugin_id);
  bool execute_tool(const std::string& plugin_id, const ToolEntry& tool);
  int primary_tool_index() const;
  void run_scenario_fallback(const std::string& plugin_id);

  std::string selected_id() const;
  std::string query_text() const;
  bool record_matches(const std::string& id, const std::string& name,
                      const std::string& description) const;
  void collect_tools(const std::string& plugin_id);
  static bool is_about_tool(const ToolEntry& tool);

  plugin::Registry* registry_ = nullptr;
  content::PluginHost* host_ = nullptr;
  Browser* browser_ = nullptr;
  ui::views::Textfield* search_ = nullptr;
  ui::views::TableView* plugins_ = nullptr;
  ui::views::ScrollView* plugin_scroll_ = nullptr;
  ui::views::View* commands_ = nullptr;
  ui::views::ScrollView* commands_scroll_ = nullptr;
  ui::views::BoxLayout* commands_box_ = nullptr;
  std::vector<ui::views::Button*> command_buttons_;
  ui::views::ScrollView* form_scroll_ = nullptr;
  ui::views::View* form_ = nullptr;
  ui::views::BoxLayout* form_box_ = nullptr;
  ui::views::Label* detail_ = nullptr;
  ui::views::Label* error_ = nullptr;
  bool presented_ = false;

  std::vector<std::string> visible_plugin_ids_;
  std::vector<ToolEntry> tool_entries_;
  std::string selected_plugin_id_;
  int selected_tool_ = -1;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PANELS_PLUGIN_CATALOG_VIEW_H_
