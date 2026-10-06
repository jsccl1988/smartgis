// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Map2d baogrid suite: IL dispatches orthogrid_run → orthogrid.scenario.showcase.

script "plugin.orthogrid" {
  seq @inproc {
    orthogrid_run();
  }
}
