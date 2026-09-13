// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_COMMANDS_H_
#define APP_VIEWS_APP_COMMANDS_H_

#include <string>

namespace app {

// Result of the chrome Open picker. BrowserView loads via MapContents
// CatalogCall when a session exists; otherwise the path is status-only.
struct OpenFileCommand {
  bool accepted = false;
  std::string path;
};

OpenFileCommand run_open_file();
void show_open_file_result(const OpenFileCommand& cmd);

}  // namespace app

#endif  // APP_VIEWS_APP_COMMANDS_H_
