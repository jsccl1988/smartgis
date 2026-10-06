// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Scene3D mine suite: IL dispatches mine_run → mine.scenario.showcase.

script "plugin.mine" {
  seq @inproc {
    mine_run();
  }
}
