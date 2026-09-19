<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP1: Legacy Tool → Workspace strangler

**Date:** 2026-09-19  
**Status:** active  
**Related:** dispatch [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md); leftover path [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md).  
**Scope:** Strangle leftover group-tool menu/`Notify` activation onto `tool::Workspace` via the existing `SmtFlashTool::bind_workspace` pattern. No rewrite of all `SmtIATool`.

## Goal

Leftover group tools keep painting / querying / appending on `apply_draft`, but **activation** of mapped `GT_MSG_*` goes through `tool::Workspace::execute` (same ids as `ViewHost::execute_legacy` / `command_id_from_gt_msg`) when a session Workspace is bound.

Concrete first wave (3 tools + Flash precedent):

| Leftover tool | Mapped commands | Local work when bound |
| --- | --- | --- |
| `SmtFlashTool` (done) | `flash.start` / `flash.stop` | Overlay paint from workspace flash flag |
| `SmtViewCtrlTool` | `view.zoom_in` / `out` / `pan` / `full` / `refresh` | Sync `m_viewMode`; run restore/refresh camera; `apply_draft` |
| `SmtSelectTool` | `selection.point` / `rect` / `polygon` / `clear` | Sync `m_selMode`; clear scratch; `apply_draft` query |
| `SmtAppendFeatureTool` | `edit.append.point` / `linestring` / `polygon` | Sync digitize kind/type; `apply_draft` append |

## Non-goals

- Rewrite all `SmtIATool` / every group tool.
- Touch `src/legacy/render/**`, `src/legacy/ui/**`, `src/legacy/app/**` (chrome bind call sites stay with UI owners; Flash already bound from `view_2d`).
- New endgame `#include "legacy/tool/t_iatool.h"` (or any leftover tool header) in `src/tool/**` / `src/content/public/**`.
- Change leftover `dll_stem` / LoadLibrary ABI.
- Qt; ViewHost large rewrites.

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| Pattern | Per-tool `bind_workspace(Workspace*)` + optional `m_workspace`, matching Flash |
| Msg → id | `tool::command_id_from_gt_msg` only (numeric enum in `legacy_msg.h`) |
| Shared helper | `tool::try_execute_gt_msg(Workspace*, long)` in endgame `legacy_msg` (no leftover include) |
| Bound activate | Call `try_execute_gt_msg`; **do not** `SetActive()` / leftover mouse capture (Workspace owns exclusive input) |
| Bound + local side effects | Keep mode sync + document-side work leftover still owns (ZoomRestore, clear_scratch, digitize kind) |
| Unbound | Existing `Notify` behavior unchanged |
| Chrome wiring | UI/ViewHost owners call `bind_workspace`; SP1 only exposes the API on tools |
| Dependency | `//src/legacy/tool/group` may dep `//src/tool:dispatch`; endgame public headers must not dep leftover tool |

## Data flow

```
Menu / plugin Notify(GT_MSG_*)
        │
        ├─ command_id_from_gt_msg ──► try_execute_gt_msg(ws)
        │                                    │
        │                                    ▼
        │                             Workspace::execute
        │                             (activate Interaction or Command)
        │
        └─ leftover local: mode flags / ZoomRestore / clear / digitize kind
                 │
Pointer ──► Workspace::dispatch_input ──► Draft ──► draft_observer
                 │
                 └─ leftover apply_draft (camera / query / append)
```

Chrome may also call `ViewHost::execute_legacy(gt_msg)` directly; both paths share the same id map.

## API

### Endgame (`src/tool/legacy_msg.h`)

```cpp
namespace tool {
class Workspace;

const char* command_id_from_gt_msg(long msg);
// False if workspace null or msg unmapped; else Workspace::execute(id).
bool try_execute_gt_msg(Workspace* workspace, long gt_msg);
}
```

### Leftover tools

```cpp
void bind_workspace(tool::Workspace* workspace) { m_workspace = workspace; }
```

`notify`: for mapped activation msgs, `try_execute_gt_msg(m_workspace, nMsg)` then local sync; skip `SetActive` when `m_workspace != nullptr`.

## Testing

- Extend `tool_dispatch_test`: `try_execute_gt_msg` null/unknown/false; mapped activate changes `stack().current()` id.
- No new MFC DLL test harness in this slice (group tools stay in `ui_legacy`).

## Done when

- Spec + plan under `docs/superpowers/`.
- Flash uses `try_execute_gt_msg`; ViewCtrl / Select / AppendFeature expose `bind_workspace` and forward mapped `GT_MSG_*`.
- `dispatch_test` covers the helper.
- No new leftover includes in endgame public headers.
