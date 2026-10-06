// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Scene3D globe suite: IL dispatches world3d_run → world3d.scenario.showcase.

script "plugin.world3d" {
  seq @inproc {
    world3d_run();
  }
}
