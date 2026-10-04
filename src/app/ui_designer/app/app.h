// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_APP_APP_H_
#define APP_UI_DESIGNER_APP_APP_H_

#include <string>

namespace app {

// Runs the UiDesigner editor message loop. Returns process exit code.
int run_ui_designer(const std::string& initial_path);

}  // namespace app

#endif  // APP_UI_DESIGNER_APP_APP_H_
