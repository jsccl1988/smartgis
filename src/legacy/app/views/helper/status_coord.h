// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_VIEWS_STATUS_COORD_H_
#define LEGACY_APP_VIEWS_STATUS_COORD_H_

#pragma once

namespace legacy_app {
namespace helper {

// Push map XY (+ lon/lat duplicate) into CMainFrame status panes.
void set_status_map_xy(float x, float y);

// Push 3D cursor XYZ into the coordinate status pane.
void set_status_scene_xyz(float x, float y, float z);

}  // namespace helper
}  // namespace legacy_app

#endif  // LEGACY_APP_VIEWS_STATUS_COORD_H_
