<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Task D brief — testing + GN + callers + docs

**Plan:** `docs/superpowers/plans/2026-09-19-ui-views-subdir-responsibility.md`  
**Do NOT commit** unless user asks.

## Preconditions

Tasks A/B/C complete: headers/sources under `kernel/` `primitives/` `dialogs/` `gis/` `map/`. No root business headers except `views.h`.

## Steps

1. `git mv` into `testing/`: pixel_harness.h/cc, pixel_png_wic.cc, views_unittests.cc, views_pixel_tests.cc, testdata/
2. Fix golden/relative paths in harness if needed
3. Rewrite `BUILD.gn` sources to new paths; test targets point at `testing/`
4. Rewrite `views.h` include list to new paths
5. Repo-wide replace old `#include "ui/views/<stem>.h"` → mapped paths (src + docs examples). Mapping table is in the plan.
6. Update README + `docs/build/ui-views-skia.md` nesting + controls design note + `ui-testing.md` / `src-layout.md` examples
7. Delete any leftover root shims if present
8. Verify: root only `views.h` among business headers; no flat `ui/views/<stem>.h` includes except via umbrella
9. If environment allows: `build.bat views` and run views_unittests / views_pixel_tests

## Owns exclusively

BUILD.gn, views.h, views.cc, src/app/views/**, testing/, docs listed above.

## Report

`.superpowers/sdd/briefs/task-D-report.md`
