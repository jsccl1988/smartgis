<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `browser/nav` — Browser navigation TUs

`Browser::` draft / extent / view-command / gesture methods. Same-class multi-TU
(like `MapPagesComposer`):

| TU | Responsibility |
| --- | --- |
| `draft_nav.cc` | `apply_nav_draft`, `handle_draft`, `forward_draft_to_contents`, blit preview |
| `extent_nav.cc` | fit / scale / extent history / `push_shared_extent` / orbit pull |
| `view_nav.cc` | `on_view_command`, shell navigation table, identify |
| `gesture_nav.cc` | pinch / gesture pan / `zoom_at_and_commit` |
| `deferred_china_seed.cc` | idle China sample open + `cancel_deferred_china_seed` |
| `detail/seh_workspace.*` | SEH ToolSession / Workspace / nav reset (shared with `ui/pages`) |
| `detail/attach_mode.h` | ContentMapView attach predicate |
| `detail/ptr_guard.h` | poison / readable pointer checks |

Parent layers (core vs China product vs harness):
[`../README.md`](../README.md).

Living shell: **§shell/ui composers** / Chromium layering in
[`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md).
