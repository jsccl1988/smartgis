// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Minimal gesture body under ui.interact.host (marks gate).

script "ui.interact.smoke" {
  // Host already seeded china + Map tab; keep this body parse/exec-light.
  window(activate);
  seq {
    pump(40);
    key(ESCAPE);
  }
  repeat 2 {
    pump(20);
  }
  mark("interact-smoke-ok") @inproc;
}
