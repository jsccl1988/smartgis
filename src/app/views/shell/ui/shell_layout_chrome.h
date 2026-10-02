// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_SHELL_LAYOUT_CHROME_H_
#define APP_VIEWS_SHELL_UI_SHELL_LAYOUT_CHROME_H_

namespace app {

class BrowserView;

// Loads shell/main_app.ui.xml and mounts real Catalog / Map / Ambox /
// inspector / Diagnostic / Status widgets into markup hosts. Falls back to
// the former imperative tree when markup fails to load.
class ShellLayoutChrome {
 public:
  explicit ShellLayoutChrome(BrowserView* host);
  ~ShellLayoutChrome() = default;

  ShellLayoutChrome(const ShellLayoutChrome&) = delete;
  ShellLayoutChrome& operator=(const ShellLayoutChrome&) = delete;

  void build_contents();

 private:
  bool build_from_markup();
  void build_imperative();

  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_SHELL_LAYOUT_CHROME_H_
