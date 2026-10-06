// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Scene3D stormsurge suite: IL dispatches stormsurge_run →
// stormsurge.scenario.showcase.

script "plugin.stormsurge" {
  seq @inproc {
    stormsurge_run();
  }
}
