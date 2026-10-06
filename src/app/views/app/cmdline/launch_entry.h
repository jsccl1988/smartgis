// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_CMDLINE_LAUNCH_ENTRY_H_
#define APP_VIEWS_APP_CMDLINE_LAUNCH_ENTRY_H_

#include <string>

namespace app {

// Raw harness CLI tokens (before id resolution). Product flags stay on
// ViewsLaunchOptions. Empty strings / false flags mean "not requested".
struct LaunchCli {
  bool self_test = false;
  bool self_test_console = false;
  bool input_showcase = false;
  bool browse_showcase = false;
  std::string atmosphere;  // land|ocean|full|coast|legacy|globe (+ aliases)
  std::string map2d;       // china|align|orthogrid (+ aliases)
  std::string plugin;      // canonical plugin id (aliases already normalized)
  std::string ui;          // shell|data|scene|catalog|interact
};

// One CLI → scenario_id matcher. Higher |rank| wins when several flags are set.
struct LaunchBinding {
  int rank = 0;
  const char* (*match)(const LaunchCli&) = nullptr;
};

void register_launch_binding(const LaunchBinding& binding);

// Resolves harness CLI to a ScenarioRegistry id. Empty = interactive product.
std::string resolve_launch_scenario(const LaunchCli& cli);

}  // namespace app

#endif  // APP_VIEWS_APP_CMDLINE_LAUNCH_ENTRY_H_
