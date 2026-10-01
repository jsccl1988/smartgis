// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Minimal DSL smoke example (marks gate).

script "ui.interact.smoke" {
  select_map_tab(0) @inproc;
  pump(150);
  window(activate);
  seq {
    pump(40);
    key(ESCAPE);
  }
  repeat 2 {
    pump(20);
  }
  mark("interact-smoke-ok") @inproc;
  select_map_tab(0) @inproc;
  pump(150);
}
