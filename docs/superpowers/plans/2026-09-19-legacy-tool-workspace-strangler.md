<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP1: Legacy Tool → Workspace strangler — plan

**Date:** 2026-09-19  
**Status:** active  
**Spec:** [`../specs/2026-09-19-legacy-tool-workspace-strangler-design.md`](../specs/2026-09-19-legacy-tool-workspace-strangler-design.md)

## Tasks

### Task 1: `try_execute_gt_msg`

**Files:** `src/tool/legacy_msg.h`, `src/tool/legacy_msg.cc`, `src/tool/dispatch_test.cc`

- [x] Add `try_execute_gt_msg(Workspace*, long)`
- [x] Tests: null / unmapped / mapped activate

### Task 2: Flash + ViewCtrl / Select / Append

**Files:** `src/legacy/tool/group/{flashtool,viewctrltool,selecttool,appendfeaturetool}.{h,cpp}`

- [x] Flash: route start/stop through `try_execute_gt_msg`
- [x] ViewCtrl / Select / Append: `bind_workspace` + notify forwarding; skip `SetActive` when bound

### Task 3: Docs

- [x] Spec + this plan

## Out of partition (siblings)

- Call `bind_workspace` from `view_2d` / edit hosts for ViewCtrl / Select / Append (Flash already wired).

## Note

`ViewHost::execute_legacy` now delegates to `try_execute_gt_msg` (one-liner; no leftover includes).
