<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Base IPC + ContentMain + standalone GPU Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One `SmartGis.exe` (or current chrome PE) relaunches itself with `--type=renderer` / `--type=gpu`; GPU process paints 2D and 3D; public API is Chromium-named `MapContents`; language is C++23.

**Architecture:** `content::ContentMain` dispatches `ProcessType`. Browser hosts `MapContents`. Renderer owns `SmtMap`/`SmtIATool`. GPU (`GpuMain`) owns GL/D3D11 + `scene3d` and returns DXGI handles. Mojo/mojom pin is a later task; v1 launch uses existing named-pipe escape hatch until `third_party/chromium` exists.

**Tech Stack:** MSVC v145, GN/`build.bat`, existing `src/content` + `src/gpu`, C++23.

**Spec:** `docs/superpowers/specs/2026-09-13-base-ipc-mojom-design.md`

## Global Constraints

- Stay on `master`; do not create topic branches.
- Copyright 2026 Mogu on new/touched engineering files.
- Multiprocess types PascalCase; `Smt_*` ABI unchanged.
- `src_all` (31 DLLs) must not compile Chromium `base` or mojo.
- One PE: children are `GetModuleFileNameW(nullptr)` + `--type=`. No new `SmartGisRender.exe`.
- GPU process is required (default). `--in-process-gpu` default off.
- 2D (`kMapEdit`/`kMapData`) and 3D (`kScene3d`) paint only in `--type=gpu`.
- Qt banned. No viz/Blink vendor.
- Build only via repo-root `build.bat`; output `out/`.
- No `Co-authored-by: Cursor`.
- Comments in English.

## File map

| Path | Responsibility |
| --- | --- |
| `build/BUILDCONFIG.gn` | `cc_std = "c++23"` on Windows |
| `build/config/win/BUILD.gn` | `/Zc:__cplusplus` |
| `src/app/winui/BUILD.gn` | drop `/std:c++17` |
| `src/content/public/process_type.h` | `ProcessType` enum + switch names |
| `src/content/app/content_main.h/.cc` | `ContentMain` / `ContentMainParams` |
| `src/content/public/map_contents.h` | replaces `map_session.h` |
| `src/content/public/map_contents_observer.h` | replaces client |
| `src/content/public/map_widget_host_view.h` | replaces `map_view.h` |
| `src/gpu/gpu_main.cc` | `GpuMain` from `--type=gpu` |
| `src/app/views/main.cc` | BrowserMain via `ContentMain` |
| Docs listed in the spec | C++23, `--type=`, GPU process |

---

### Task 1: Repo C++23

**Files:**
- Modify: `build/BUILDCONFIG.gn` (`cc_std`)
- Modify: `build/config/win/BUILD.gn` (`cflags_cc` ` /Zc:__cplusplus`)
- Modify: `src/app/winui/BUILD.gn` (remove `/std:c++17`)
- Modify: `README.md`, `build/README.md` (C++23)
- Fix: any `error:` from `build.bat` under C++23

**Interfaces:**
- Produces: all product TUs compile with MSVC `/std:c++23`

- [x] **Step 1:** Set Windows `cc_std = "c++23"` (keep `c_std = "c17"`).
- [x] **Step 2:** Add `/Zc:__cplusplus` to `build/config/win` default `cflags_cc`.
- [x] **Step 3:** Remove WinUI `/std:c++17` so it inherits repo `cc_std`.
- [x] **Step 4:** Run `build.bat` from repo root. Parse `out/build.log`. Fix conformance in place (no second `cc_std` for `smt_shared_library`). Isolate only a third-party/WinRT target if a header cannot compile as C++23.
- [x] **Step 5:** `build.bat` exit 0, no `FAILED:`.
- [x] **Step 6:** Commit `Bump the tree to C++23.`

---

### Task 2: ProcessType + ContentMain

**Files:**
- Create: `src/content/public/process_type.h`
- Create: `src/content/app/content_main.h`
- Create: `src/content/app/content_main.cc`
- Modify: `src/content/BUILD.gn` (add sources; still no Chromium deps)

**Interfaces:**
- Produces:

```cpp
namespace content {
enum class ProcessType { kBrowser, kRenderer, kGpu, kUtility };
ProcessType ProcessTypeFromCommandLine(int argc, wchar_t** argv);
const wchar_t* ProcessTypeSwitchValue(ProcessType t);  // L"renderer" etc.

struct ContentMainParams {
  HINSTANCE instance = nullptr;
  int argc = 0;
  wchar_t** argv = nullptr;
};
int ContentMain(const ContentMainParams& params);
}
```

`--type=` values: omit / `browser` → `kBrowser`; `renderer`; `gpu`; `utility`.

`ContentMain` calls `mojo::core::Init` only when Chromium is linked; until then it only dispatches:

```cpp
int ContentMain(const ContentMainParams& p) {
  switch (ProcessTypeFromCommandLine(p.argc, p.argv)) {
    case ProcessType::kGpu: return gpu::render_main(p.argc, p.argv);
    case ProcessType::kRenderer: return gpu::render_main(p.argc, p.argv); // temporary: same payload until split
    case ProcessType::kUtility: return 0;
    case ProcessType::kBrowser: default: return 1; // browser host fills this
  }
}
```

Until Task 4, `kRenderer` may share `gpu::render_main` (escape hatch). GPU self-test must still refuse a D3D device in a later split; Task 4 enforces renderer has no GPU device.

- [x] **Step 1:** Add headers + `.cc` with the signatures above.
- [x] **Step 2:** Add to `//src/content:content` sources.
- [x] **Step 3:** Commit `Add ContentMain process-type dispatch.`

---

### Task 3: Same-PE child launch (Views chrome)

**Files:**
- Modify: `src/app/views/main.cc` — `wWinMain` → `content::ContentMain`; browser path keeps existing Views UI
- Modify: `src/content/map_session.cc` `start_render_process` — `CreateProcess` **this module**, not `SmartGisRender.exe`

Command line:

```
L"\"%s\" --type=gpu --parent-pid=%u --session=%s"
```

plus existing `--pipe=` escape hatch until Mojo. Second child `--type=renderer` can wait until Task 4; **must** launch `--type=gpu` now.

Use `GetModuleFileNameW(nullptr)` for `exe`. `CREATE_NO_WINDOW` on children. Job `KILL_ON_JOB_CLOSE`.

- [x] **Step 1:** Views `wWinMain` if `--type=` is gpu/renderer, `ContentMain` and return (do not create Views widget).
- [x] **Step 2:** `MapSessionImpl::start_render_process` launches self `--type=gpu`.
- [x] **Step 3:** `build.bat views` (or `smt_build_views=true`) and `out\SmartGisViews.exe --type=gpu --self-test` if argv is plumbed; otherwise `gpu::run_self_test`.
- [x] **Step 4:** Commit `Relaunch the chrome PE as --type=gpu.`

---

### Task 4: Renderer vs GPU split in-process APIs

**Files:**
- Modify: `src/gpu/gpu.h`, `gpu_main.cc`, `self_test.cc`
- `gpu::GpuMain` = present + `SmtRender`/`scene3d`
- `content` renderer main = session/tools only; **must not** call `D3D11CreateDevice`

`--type=gpu --self-test`: creates device + shared handle, no browser HWND.  
`--type=renderer --self-test`: no GPU device (assert / return 0).

- [x] **Step 1:** Split `render_main` into `GpuMain` vs renderer stub that connects pipe as today.
- [x] **Step 2:** Self-tests as above.
- [x] **Step 3:** Commit `Split GpuMain from renderer entry.`

---

### Task 5: Chromium public names

**Files:**
- Create: `map_contents.h`, `map_contents_observer.h`, `map_widget_host_view.h`
- Modify: hosts under `src/app/{views,webview2,winui}` includes
- Delete: `map_session.h`, `map_view.h`, `tool_router.h` after hosts compile

`MapContents::Create()` replaces `create_map_session()`. Methods PascalCase on new types; implementation can wrap existing `snake_case` until Mojo.

- [ ] **Step 1:** New headers + `map_contents.cc` (move from `map_session.cc`).
- [ ] **Step 2:** Update three hosts.
- [ ] **Step 3:** `build.bat views` / `web` / `winui` as flags allow.
- [ ] **Step 4:** Commit `Rename MapSession to MapContents.`

---

### Task 6: Docs

**Files:** spec's "Docs to update" list + `docs/README.md` plan link.

- [x] **Step 1:** `ui-shell-multiprocess.md` §0.2/0.4: Browser+Renderer+GPU, one PE, `--type=`.
- [x] **Step 2:** `src-layout.md`, `README.md`, `build/README.md` C++23 and `ContentMain`.
- [x] **Step 3:** Commit `Document ContentMain and the GPU process.`

---

### Task 7: Mojo pin + mojom (blocked without chromium tree)

**Files:** `third_party/chromium/`, `build/mojom.gni`, `src/content/public/mojom/map_widget.mojom`, `gpu.mojom`

If the pin cannot be fetched, stop this task with BLOCKED and keep named-pipe escape hatch. Do not invent a second IDL.

- [ ] Sparse-pin chromium `base`+`mojo` or BLOCKED.
- [ ] `mojom.gni` cpp_only + generate `map_widget.mojom` / `gpu.mojom`.
- [ ] Invitation replaces `--pipe=` on the Mojo path.

## Coverage vs spec

| Spec | Task |
| --- | --- |
| C++23 whole repo | 1 |
| ContentMain `--type=` | 2 |
| Same PE children | 3 |
| Standalone GPU 2D+3D | 4 (3D backend still existing `scene3d` in GpuMain) |
| MapContents names | 5 |
| Docs | 6 |
| Mojo/mojom v1 | 7 (may BLOCK) |
| UI event Forward* | 5 wrap + existing `dispatch` |
| Traits/typemaps | 7 with mojom |

Do not vendor viz. Do not add Qt.
