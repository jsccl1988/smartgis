// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_DEBUG_CONSOLE_CHROME_H_
#define APP_VIEWS_SHELL_UI_PANELS_DEBUG_CONSOLE_CHROME_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// Diagnostic Tools / DebugAgent chrome wire.
class DebugConsoleChrome {
 public:
  explicit DebugConsoleChrome(BrowserView* host);
  ~DebugConsoleChrome() = default;

  DebugConsoleChrome(const DebugConsoleChrome&) = delete;
  DebugConsoleChrome& operator=(const DebugConsoleChrome&) = delete;

  void bind_debug_agent_host();
  void wire_debug_console();
  void toggle_debug_console();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_DEBUG_CONSOLE_CHROME_H_
