// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_APP_STARTUP_POLICY_H_
#define APP_VIEWS_APP_STARTUP_POLICY_H_

#include <string_view>

#include "app/views/app/startup/scenario.h"

namespace app {

// Cold-start product policy: FlyCube map2d (align carto gold); Scene3d FlyCube.
// Scenic 2D via MAP2D_ENGINE=scenic. Bare SmartGIS.exe always applies this.
LaunchPolicy product_startup_policy();

// Policy for an explicit ScenarioRegistry id (panel Run / HARNESS_SUITE).
// Unknown ids fall back to a GDI harness face — never used for cold start.
LaunchPolicy startup_policy_for_scenario(std::string_view scenario_id);

// Scene3d / Map2d engine gates and overlay switches before Browser::init.
void apply_startup_policy(const LaunchPolicy& policy);

}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_POLICY_H_
