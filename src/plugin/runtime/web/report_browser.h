// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_WEB_REPORT_BROWSER_H_
#define PLUGIN_RUNTIME_WEB_REPORT_BROWSER_H_

#include <string>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "plugin/runtime/host/plugin_host_export.h"

namespace plugin {

// Embeds a local HTML report pack (index.html + assets). Backends: WebView2
// (v1), optional CEF later. Not a product horizon shell.
class PLUGIN_HOST_EXPORT ReportBrowser {
 public:
  virtual ~ReportBrowser() = default;

  // Parent HWND that owns the browser surface (panel native child).
  virtual void set_parent_hwnd(HWND parent) = 0;
  virtual void set_bounds(int x, int y, int width, int height) = 0;

  // Directory containing index.html (and usually data.json + js/).
  virtual bool navigate(std::string_view report_dir) = 0;
  virtual bool post_json(std::string_view json) = 0;
  virtual void close() = 0;

  // Soft-fail when Runtime / backend is unavailable.
  virtual bool is_available() const = 0;
  virtual std::string status_text() const = 0;

  void add_allowed_root(std::string root);
  void clear_allowed_roots();
  bool is_path_allowed(std::string_view path) const;

  // file:/// URL for index.html under |report_dir|, or empty on failure.
  static std::string file_url_for_report_dir(std::string_view report_dir);
  static bool is_blocked_navigation_url(std::string_view url);

 protected:
  std::vector<std::string> allowed_roots_;
};

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_WEB_REPORT_BROWSER_H_
