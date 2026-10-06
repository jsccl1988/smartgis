// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Scene3D orthogrid3d suite: IL dispatches orthogrid3d_run →
// orthogrid3d.scenario.showcase.

script "plugin.orthogrid3d" {
  seq @inproc {
    orthogrid3d_run();
  }
}
