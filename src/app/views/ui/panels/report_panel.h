// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PANELS_REPORT_PANEL_H_
#define APP_VIEWS_UI_PANELS_REPORT_PANEL_H_

#include <memory>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/views/kernel/view/view.h"

namespace plugin {
class ReportBrowser;
}

namespace ui {
namespace views {
class Label;
}
}  // namespace ui

namespace app {

// Inspector Report tab: hosts plugin::ReportBrowser (WebView2 v1) for local
// HTML report packs. Lives in the shell to avoid ui_views ↔ plugin_host cycles.
class ReportPanel : public ui::views::View {
 public:
  ReportPanel();
  ~ReportPanel() override;

  ReportPanel(const ReportPanel&) = delete;
  ReportPanel& operator=(const ReportPanel&) = delete;

  void set_browser(std::unique_ptr<plugin::ReportBrowser> browser);
  plugin::ReportBrowser* browser() const { return browser_.get(); }

  bool open_report(std::string_view report_dir);
  bool post_json(std::string_view json);
  void close_report();

  void add_allowed_root(std::string root);
  void set_status_text(std::string text);

  void layout() override;

 protected:
  HWND create_native_view(HWND parent) override;

 private:
  bool ensure_host_hwnd();
  void sync_browser_bounds();
  void refresh_status();

  ui::views::Label* status_ = nullptr;
  std::unique_ptr<plugin::ReportBrowser> browser_;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PANELS_REPORT_PANEL_H_
