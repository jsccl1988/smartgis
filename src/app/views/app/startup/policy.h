// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_STARTUP_POLICY_H_
#define APP_VIEWS_APP_STARTUP_POLICY_H_

#include <string_view>

#include "app/views/app/startup/scenario.h"

namespace app {

// Interactive product (empty scenario_id): FlyCube/Vista 2D GPU SoT;
// Scene3d engine left unchanged (env / View menu).
LaunchPolicy product_startup_policy();

// Policy registered on the scenario, or a GDI harness fallback for unknown ids.
LaunchPolicy startup_policy_for_scenario(std::string_view scenario_id);

// Scene3d / Map2d engine gates and overlay switches before Browser::init.
void apply_startup_policy(const LaunchPolicy& policy);

}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_POLICY_H_
