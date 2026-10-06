// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin print.preview: IL dispatches map2d_print_run →
// map2d.scenario.print.

script "plugin.print" {
  seq @inproc {
    map2d_print_run();
  }
}
