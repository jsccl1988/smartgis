<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Root-cause casebook (smartgis / auto-diagnose-fix)

Living ledger of **confirmed** crash/hang root causes. Newest cases first.

**Agent must:**

1. After parse (exception / hang stacks + digest), **read this file** and match
   fingerprint / top frames / module before shotgun edits.
2. When root cause is confirmed (fix landed, or agreed and deferred), **append**
   a new case at the top of the Case log (keep the Index row in sync).
3. Do **not** delete old cases; set `Status:` to `fixed` / `open` / `wontfix`.
4. Keep entries short; link `out/crash/*` logs instead of pasting full `!analyze`.

## Index (quick match)

| Fingerprint / symptom | Exception | Top symbol / module | Status | Case |
| --- | --- | --- | --- | --- |
| Release `browser.harness` → `0xC0000409` / `e06d7363` during `find_fn` | `e06d7363` / abort | `register_fn` / `install_script_verbs` / `msvcp140!_Throw_Cpp_error` | fixed | [2026-10-23-b](#case-2026-10-23-find-fn-overlay-mutex-reentry) |
| Cold start → `DrawHost::attach` → `enqueue_display_task` / deque `_Orphan_all` | `c0000005` | `std::exchange` / `deque::push_back` / `ui_views_d` | fixed | [2026-10-23](#case-2026-10-23-shell-stl-dll-cascade) |
| Cold start → `mount_fill` / Yoga `~unordered_map` free | `HEAP_CORRUPTION` / `c0000005` | `YogaLayoutManager::~` ← `set_layout_manager` ← `mount_fill` | fixed | [2026-10-23](#case-2026-10-23-shell-stl-dll-cascade) |
| Cold start → `wire_catalog` → later `fill_manifest` / plugin vector | `HEAP_CORRUPTION` / `80000003` | `CommandContrib::push_back` / `plugin_host_d` | fixed | [2026-10-23](#case-2026-10-23-shell-stl-dll-cascade) |
| Cold start → `set_contents_view` → DrawHost HWND `clear_shell_overlay` | `c0000005` | `std::exchange` / `vector::clear` / `DrawHost::clear_shell_overlay` | fixed | [2026-10-23](#case-2026-10-23-shell-stl-dll-cascade) |
| Cold start → after shell attach → `base::log_write` / `LogEntry::~` | `c0000374` / `80000003` | `basic_string::~` ← `log_write` ← `Browser::init` | fixed (latent) | [2026-10-23](#case-2026-10-23-shell-stl-dll-cascade) |
| Cold start: HWND vis=True but behind IDE/browser; “没看到窗口” | (none) | `Widget::show` / `ShowWindow(SW_SHOW)` only | fixed | [2026-10-22-b](#case-2026-10-22-shell-show-no-foreground) |
| Cold start → `init_shell` → Ambox/Label/GDI; HEAP_CORRUPTION | `c0000374` | `MarkupDocument::~` / `AmboxView::GroupBlock` / `ui_views_d` | fixed | [2026-10-22](#case-2026-10-22-views-markup-ambox-stl-dll) |
| Cold start → plugin scan → contribute index; `std::string` `this==0xF` | `c0000005` | `index_manifest_locked` / `plugin_host_d` | fixed | [2026-10-21](#case-2026-10-21-plugin-host-stl-dll-boundary) |
| Heap corrupt during `ProcessingContrib::push_back` in scan | `c0000374` | `fill_manifest` / `plugin_host_d` | fixed | [2026-10-21](#case-2026-10-21-plugin-host-stl-dll-boundary) |

## Case template (copy)

```markdown
## Case: YYYY-MM-DD short-kebab-title

- **Status:** open | fixed | wontfix
- **Fingerprint:** FAILURE_BUCKET_ID or `exception + module + top3 frames`
- **Symptom:** one line (user / harness repro)
- **Evidence:** `out/crash/<stamp>-catch.log`, `…-analyze.log`, digest
- **Root cause:** mechanism (not just faulting line)
- **Fix:** file/API change, or “deferred: …”
- **Guardrail:** how to avoid / detect next time
- **False leads:** what looked true but was not
```

---

## Case log

### Case: 2026-10-23-b find-fn-overlay-mutex-reentry

- **Status:** fixed (2026-10-23: Release `browser.harness` reaches marks past
  `hwnd-ok`; cold `find_fn("mark")` no longer aborts)
- **Fingerprint:** `CPP_EXCEPTION_e06d7363_msvcp140.dll!std::_Throw_Cpp_error`
  + `install_script_verbs` ← `install_scene_verbs` ← `registry` ← `find_fn`
- **Symptom:** `build.bat release harness` / `loop_runner --suite browser.harness`
  exits `3221226505` (`0xC0000409`) with empty marks after ~30–50s
- **Evidence:** `out/crash/20261023-034722-catch.*`,
  `out/crash/20261023-034759-analyze.log`
- **Root cause:** `content::py::find_fn` held `Overlay::mu` while calling
  `code::find_op` → lazy `build_registry` → `install_host_verbs` →
  `tile::install_script_verbs` → `register_fn`, which locks the same
  non-recursive `std::mutex` → `resource_deadlock_would_occur` → uncaught
  C++ EH → process abort (often reported as `0xC0000409`)
- **Fix:** Split the lock in `find_fn` / `unregister_fn` so `find_op` /
  `register_fn` never run under `overlay().mu`. Regression test
  `PluginFn.FindFnSafeAcrossColdRegistryInstall`
- **Guardrail:** Never call `find_op` / `ensure_ops_registered` /
  `install_*_verbs` while holding `Overlay::mu`
- **False leads:** Blaming Debug STL EXE↔`ui_views` cascade (different
  dump / Debug-only); treating `CURSOR_API_KEY` peel as the abort root
  (it only affected a later `wait_viewport` miss after this fix)

### Case: 2026-10-23 shell-stl-dll-cascade

- **Status:** fixed (2026-10-23: cold start survives 90s under cdb —
  `FirstShellPaint` + `map2d.present`; native 15s ALIVE)
- **Fingerprint:** cascade — (1) Yoga `~unordered_map`; (2) plugin
  `fill_manifest` / `push_back`; (3) `DrawHost::clear_shell_overlay`
  `vector` `_Orphan_all`; (4) `DrawHost::enqueue_display_task` →
  `deque<DisplayTask>::push_back` → `std::exchange` /
  `INVALID_POINTER_READ_*_ui_views_d.dll!std::exchange`
- **Symptom:** `out/Debug/SmartGis.exe` cold start dies in `init_shell` /
  first `DrawHost::attach` GPU present enqueue
- **Evidence:** `out/crash/20261023-000412-catch.*` (Yoga);
  `20261023-005043-catch.*` (plugin fill_manifest);
  `20261023-010809-catch.*` (DrawHost clear / set_contents_view);
  `20261023-014412-catch.*` + `20261023-014522-analyze.log` (display
  deque); verify `20261023-020436-catch.log` (timeout, no dump)
- **Root cause:** Debug STL is not EXE↔`ui_views.dll` ABI. Shell composer
  passed `unique_ptr<LayoutManager>`, `std::function`, and `std::string` /
  `vector<string>` from SmartGIS.exe into toolkit methods; heap poison /
  Debug iterator proxies also fire on **member** STL containers
  (`vector` shell overlay, `deque` display mailbox) even after DLL
  `make_draw_host`.
- **Fix:** Landed —
  - `install_fill_layout` / `install_box_layout`; `mount_fill` uses
    `remove_all_children` + install (no EXE `make_unique<FillLayout>`, no
    EXE-side `remove_child` destroy of markup kids)
  - `TabStrip::add_tab(const char*)` + `set_change_fn`
  - `CatalogView` / `LayerTree` `*_fn` + `set_source_names_c` / `add_map_doc`
  - `FrameView::set_title(const char*)`, `StatusBar` text setters `const char*`
  - `make_draw_host` / `make_frame_view` / `make_status_bar` (DLL alloc)
  - `shell_bgra_` → `unique_ptr<uint8_t[]>` (no Debug `vector` orphan on clear)
  - `display_queue_` → POD ring (`display_ring_[32]`) — no Debug `deque`
    `_Orphan_all` on first `enqueue_display_task` from `try_gpu_present_device`
- **Guardrail:** Any new EXE→`ui_views.dll` API: no by-value `std::string` /
  `vector` / `std::function` / `unique_ptr` of toolkit types; prefer
  `const char*` + fn+baton + DLL-side `install_*` / `make_*`. Avoid
  Debug `vector` / `deque` **members** on types that cross module ownership
  (prefer POD rings / `unique_ptr<T[]>`).
- **False leads:** Blaming `fill_manifest` / RapidJSON alone (detector, not
  first poison); treating StatusBar/FrameView title as unrelated to DrawHost;
  EXE `make_unique<DrawHost>` alone (factory insufficient until `shell_bgra_`
  left `vector`); treating `log_write` AV as the remaining root (cleared once
  earlier poison / overlay deque paths were fixed).

### Case: 2026-10-22-b shell-show-no-foreground

- **Status:** fixed (2026-10-22: after `bring_widget_to_foreground` in
  `Widget::show`, cold start reaches `FG_APP=True` ~19s —
  `SmartGisViewsWidget` / `SmartGIS Views`)
- **Fingerprint:** no exception; HWND `SmartGisViewsWidget` `vis=True`
  1280×800 @ (0,0); foreground stays on Cursor/Chrome/Unreal
- **Symptom:** User launches `out/Debug/SmartGIS.exe` and reports 没看到窗口;
  process alive, map2d present logs OK
- **Evidence:** HWND enum (vis=True, never fg); desktop screenshot covered by
  browser; hang dump during “未响应” ghost was busy
  `finish_deferred_shell_wiring` → `AttributeTable::set_rows` (UI pump blocked),
  not a true deadlock
- **Root cause:** `Widget::show` only `ShowWindow(SW_SHOW)`. Windows focus-steal
  rules leave the custom-frame shell behind the previous foreground app.
  Secondary: deferred inspector sync blocks the UI thread for ~10–20s after
  show (Ghost “未响应”), which makes a behind-window even harder to notice.
- **Fix:** Landed — `bring_widget_to_foreground` after `ShowWindow` in
  `widget_lifecycle.cc` (`SetForegroundWindow`, Alt unlock via `SendInput`,
  brief `HWND_TOPMOST`). Do **not** use `AttachThreadInput` (deadlocks vs
  Unreal/other UI threads).
- **Guardrail:** Top-level product show paths must activate; enumerate HWND
  `vis` **and** `GetForegroundWindow` pid when diagnosing “no window”.
- **False leads:** Treating “未响应” Ghost as a hang deadlock; assuming crash
  because the shell is not on top of the IDE.

### Case: 2026-10-22 views-markup-ambox-stl-dll

- **Status:** fixed (2026-10-22: cold start survives 55s under cdb with map2d
  present / FirstShellPaint; `run_and_catch` timed out with no dump —
  `out/crash/20261022-102905-catch.log`)
- **Fingerprint:** `HEAP_CORRUPTION_*` / `c0000374`; frames
  `MarkupDocument::~` ← `load_markup_bytes` ← `AmboxView::AmboxView` (1st),
  then after markup fix: `GroupBlock` ← `populate_from_commands` ←
  `AmboxComposer::populate_ambox` (2nd)
- **Symptom:** `out/Debug/SmartGIS.exe` cold start dies in `init_shell` /
  shell layout (no user click)
- **Evidence:**
  - `out/crash/20261022-100546-catch.log` / `.dmp` (pugixml free after
    EXE `load_markup` return)
  - `out/crash/20261022-102321-catch.log` / `.dmp` (Label/GDI free after
    EXE `populate_from_commands(vector<Group>)`)
- **Root cause:** Same class as plugin-host STL boundary — Debug STL is not
  a DLL ABI. (1) EXE called `load_markup` returning `MarkupRoot` by value
  from `ui_views.dll` (header already forbade this; NRVO / mismatched slot).
  (2) EXE `AmboxComposer` passed `std::vector<AmboxView::Group>` (and
  `std::function` handlers) into DLL methods; heap poison detected later
  on free (often pugixml or GDI).
- **Fix:** Landed —
  - `ShellLayoutComposer::build_from_markup` → `load_markup_into`
  - `AmboxView::populate_from_catalogs` / `replace_groups_*` /
    `set_command_handler_fn`; EXE composers use those
- **Guardrail:** Cross-DLL into `ui_views.dll`: no returning / taking
  `MarkupRoot` / `vector<Group>` / `std::function` by value. Prefer
  `load_markup_into`, char* group builders, fn+baton.
- **False leads:** Blaming pugixml / GDI font paths alone (symptom of earlier
  EXE↔DLL STL corruption); treating Ambox ctor markup load as the only fault.

### Case: 2026-10-21 plugin-host-stl-dll-boundary

- **Status:** fixed (2026-10-21: cold start survives 45s under cdb with map2d
  present; no second-chance. Verify log `out/crash/20261021-003225-catch.log`)
- **Fingerprint:** `c0000005` read `0x2f`; `plugin_host_d!_Large_mode_engaged`;
  `this == 0x0f`; frames
  `index_manifest_locked` ← `rebuild_contribute_index` ←
  `PluginManager::scan_directory` ← `PluginShell::ensure_discovered`
- **Symptom:** `out/Debug/SmartGis.exe` cold start dies in ~3–7s while scanning
  `out/Debug/plugins` (no user click required)
- **Evidence:**
  - `out/crash/20261021-000113-catch.log` (authoritative live `kn`)
  - `out/crash/20261021-000113-catch.dmp`
  - `out/crash/20261021-000135-analyze.log` (`MEMORY_CORRUPTION_*` / corrupt
    `.ecxr` — prefer catch.log stack over offline `.ecxr` when they disagree)
  - Agent NDJSON `debug-778a24.log`: `scan_exit n=8` then crash in rebuild
- **Root cause:** MSVC **Debug STL is not a DLL ABI**. EXE called
  `PluginManager::scan_directory(const std::string&)` with an EXE-owned
  string; DLL methods touch that object / poison the heap. Crash surfaces later
  in `rebuild_contribute_index` when converting contrib `std::string` →
  `string_view` (`this==0xF`, SSO threshold smell). Same class of bug as
  `Registry::list()` returning `vector<PluginRecord>` / assigning `Manifest`
  across the EXE↔`plugin_host` boundary (see comment on
  `Registry::add_builtin_manifest`).
- **Fix:** Landed —
  - `PluginManager::scan_directory(const char*)` + DLL-local path copy
  - `Registry::visit` / `for_each_record` / `apply_startup` (char buffers)
  - EXE shell/ambox/startup_apply no longer call `list()` across the boundary
  - `rebuild_contribute_index` uses `for_each_record`
  - Removed `debug-778a24` agent instrumentation
  - Note: `plugin_catalog_view` still uses `list()` / `last_error()` string
    assign — fix before relying on Plugin Manager UI under Debug
- **Guardrail:** Any new `PLUGIN_HOST_EXPORT` that takes/returns
  `std::string` / `std::vector<…string…>` / `Manifest` by value or const-ref
  from SmartGIS.exe is suspect. Match `scan.h` “same-module only” notes.
- **False leads:** Blaming a single `plugin.json` / `processing` push_back alone
  (scan can complete all 8 packs then die on index rebuild); trusting dump
  `.ecxr` when instruction bytes show `fe ???` but catch.log had a clean
  `cmp [rax+20h],0Fh` stack.
