// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PANELS_DEBUG_CONSOLE_COMPOSER_H_
#define APP_VIEWS_UI_PANELS_DEBUG_CONSOLE_COMPOSER_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// Diagnostic Tools / DebugAgent horizon wire.
class DebugConsoleComposer {
 public:
  explicit DebugConsoleComposer(BrowserView* host);
  ~DebugConsoleComposer() = default;

  DebugConsoleComposer(const DebugConsoleComposer&) = delete;
  DebugConsoleComposer& operator=(const DebugConsoleComposer&) = delete;

  void bind_debug_agent_host();
  void wire_debug_console();
  void toggle_debug_console();

 private:
  // Thin entry for DebugAgent ui_open_panel (must stay SBO-sized: capture this
  // only). Marshals onto the UI thread — see .cc.
  std::string open_panel_for_agent(const std::string& panel);
  // Runs |body| on the shell UI thread. |body| is stack-owned for the call.
  std::string invoke_on_ui(const std::function<std::string()>& body);

  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PANELS_DEBUG_CONSOLE_COMPOSER_H_
