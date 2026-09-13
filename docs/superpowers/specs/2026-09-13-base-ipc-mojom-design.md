<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Base IPC + Chromium Mojo / mojom (OOP map)

**Date:** 2026-09-13  
**Status:** draft (awaiting review)  
**Scope:** one implementation plan. Move host transport out of `content/common/ipc.h` into `src/base/ipc`, adopt Chromium Mojo (static embedder + **mojom generator in v1**), name the multiprocess map stack like Chromium `WebContents` / `RenderProcessHost` / `Widget`, and use **one PE with Chromium `--type=` entry points** (no `SmartGisRender.exe`).

## Goal

Windows OOP map rendering uses **real Chromium Mojo** and Chromium's **single-binary multiprocess** model: one `SmartGis.exe`, `content::ContentMain`, children relaunch the same image with `--type=`. A **standalone GPU process** (`--type=gpu`) owns all **2D and 3D** map painting (GL / D3D11 / scene3d). The browser only presents. Language floor is **C++23 for the whole tree**.

## Non-goals

- Do not vendor Blink, Chromium `content/`, viz, Chromium `ui/views`, or Skia wholesale.
- Do not put Chromium `base/` on the include path of `SmtCore` / `src_all` 31 DLLs.
- Do not ship a second map image (`SmartGisRender.exe`). Children are `SmartGis.exe --type=…`.
- Do not run `SmtRenderDevice`, GL, D3D11, or 3D engines in the browser process.
- Do not put 2D and 3D on different GPU processes (one GPU process, N surfaces, mixed `ViewKind`).
- Do not replace present with Chromium viz / command buffer.
- Do not put `kIoCall` on the map widget pipe (that is `--type=utility` in v1.5).
- Do not keep expanding `HostMsg` / `FrameHeader` on the Mojo path.
- Qt is banned.

## Language (whole repo)

Windows `declare_args` `cc_std` becomes **`c++23`**, same as the non-Windows branch in `build/BUILDCONFIG.gn`. `c_std` stays `c17` on Windows unless a separate change needs C23.

- Remove per-target overrides that pin `/std:c++17` (notably `src/app/winui/BUILD.gn` `cflags_cc`).
- Enable `/Zc:__cplusplus` on MSVC if not already on, so `__cplusplus` is not stuck at 199711.
- Legacy `Smt_*` TUs compile as C++23; fix conformance errors in place. Do not leave a second `cc_std` for `smt_shared_library`.
- If a single third-party or WinRT header cannot compile as C++23, isolate **that target** with an explicit flag and document it; do not roll the repo back to C++17.

Traits in this spec may use C++20 concepts, `requires`, `if constexpr`, and C++23 that MSVC v145 accepts. If MSVC ICEs on one construct, replace that **instantiation** with an explicit specialization; do not fork the protocol.

## Architecture

One product image, Chromium `ContentMain` dispatch:

```
out/SmartGis.exe                          # same PE
  (default / --type=browser)             BrowserMain — Views chrome
  --type=renderer                         RendererMain — SmtMap / SmtIATool
  --type=gpu                              GpuMain — D3D11/GL present
  --type=utility                          UtilityMain — reserved (IO/SDE)
```

```
Browser (no --type)
  MapContents / MapWidgetHostView          present only
  RendererProcessHost + GpuProcessHost     two children, always
       │
       ├─ SmartGis.exe --type=renderer     SmtMap / SmtIATool (CPU)
       │    MapWidget  (input, tools, catalog)
       │
       └─ SmartGis.exe --type=gpu         2D + 3D paint + DXGI handle
            Gpu / GpuHost                  one device, N surfaces
```

`--ui=views|web|winui` is **browser-only**. Child processes never load WebView2, WinUI, MFC, or Views chrome. Leftover `build.bat app` MFC exe is not the multiprocess image.

`RendererProcessHost::Launch` / `GpuProcessHost::Launch` use `GetModuleFileNameW(nullptr)`, copy the browser command line, set `--type=renderer` or `--type=gpu`, append `--mojo-platform-channel-handle=`. Never a different `output_name`. Product topology is **always two children** (Chromium Windows). `--in-process-gpu` is debug-only and **default off**.

## Single binary / ContentMain

```
wWinMain → content::ContentMain(ContentMainParams)
             CommandLine --type=
             kBrowser   → BrowserMain
             kRenderer  → RendererMain
             kGpu       → GpuMain
             kUtility   → UtilityMain
```

Switch names match Chromium: `--type=renderer`, `--type=gpu`, `--type=utility`. Browser is the default (omit `--type` or `--type=browser`).

| Chromium | This repo |
| --- | --- |
| `chrome.exe` | `out/SmartGis.exe` |
| `--type=renderer` | `SmtMap` + `SmtIATool` only. **No** D3D/GL device |
| `--type=gpu` | **Required** child. One GPU device paints **2D and 3D** (`kMapEdit` / `kMapData` / `kScene3d`) |
| `--type=utility` | future IO/SDE payload |
| `--in-process-gpu` | GPU Main inside renderer. **Default off.** Debug / CI only |
| `--in-process-renderer` | all Mains in the browser (dev only, default off) |

Default launch: Browser starts **Renderer and GPU**. `GpuProcessHost` and `RendererProcessHost` both exist in v1. Present and `Smt*Render*` / `scene3d` / `terrain` / `pointcloud` run only in `GpuMain`.

GN: one `executable("smartgis")` links browser + renderer + gpu + utility mains (like Chromium `chrome`). `src_all` stays 31 DLLs and does not link this exe. `build.bat render` becomes `out\SmartGis.exe --type=renderer --self-test` (and `--type=gpu --self-test`). Retire `//src/gpu:gpu` as a separate `console_app`.

Same-revision parent and child **statically** link `mojo::core::Init` + IO thread + `ScopedIPCSupport`. Dynamic `mojo_core.dll` is not v1.

Escape hatch: if the Chromium pin is missing, freeze today's named pipe + `HostMsg` and do not run it at the same time as an invitation. New fields are added only in `.mojom`.

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `base::ipc` (`src/base/ipc`) | Invitation / message-pipe / platform-handle facade; Chromium include dirs **private** | `//third_party/chromium` (ipc targets only) |
| `content::MapContents` | Public session API (today `MapSession`) | `MapRenderProcessHost` |
| `MapContentsObserver` | Frame / extent / death callbacks | none (chrome implements) |
| `MapWidgetHostView` | Public viewport; `latest()` shared surface | `MapWidgetHost` |
| `content::ContentMain` | `wWinMain` dispatch on `--type=` | app + content |
| `RendererProcessHost` | Launch `SmartGis.exe --type=renderer`, Job, invitation | `base::ipc` |
| `GpuProcessHost` | Always launch `--type=gpu`; TDR restarts this process only | `base::ipc` |
| `MapWidgetHost` | Per-view proxy; `Forward*` input to **renderer** | `Remote<MapWidget>` |
| `content.mojom` | `MapWidget` / `MapWidgetHost` | slim `build/mojom.gni` |
| `gpu.mojom` | `Gpu` / `GpuHost` — surfaces, 2D/3D paint, `FrameReady` | slim `build/mojom.gni` |
| `gpu::GpuMain` | `--type=gpu` entry; 2D+3D backends | `src/gpu`, `src/render/*` |

`MapRenderProcessHost` is an alias for `RendererProcessHost` during the rename; do not keep both in public headers.

Public headers stay under `src/content/public/` (no `public/browser/` third nest). Includes look Chromium-like: `"content/public/map_contents.h"`.

`//src:src_all` does **not** depend on Chromium `base` or mojom generation. Chromium `base` + mojo link only into `SmartGis.exe`.

## Chromium naming

| Retired | v1 |
| --- | --- |
| `content::MapSession` | `content::MapContents` |
| `MapSessionClient` | `content::MapContentsObserver` |
| `create_map_session()` | `MapContents::Create()` |
| `MapView` | `content::MapWidgetHostView` |
| hidden pipe owner | `content::RendererProcessHost` (`GpuProcessHost` for `--type=gpu`) |
| per `view_id` | `content::MapWidgetHost` |
| `ToolRouter` | methods on `MapWidgetHost` (optional typedef during the move) |
| mojom `MapHost` (UI→R) | `content.mojom.MapWidget` |
| mojom `MapClient` (R→UI) | `content.mojom.MapWidgetHost` |
| `gpu` surface slot | `gpu::GpuMain` + `gpu.mojom.Gpu` |

Delete `map_session.h` / `map_view.h` / `tool_router.h` after hosts are updated. No long-lived aliases.

**Method names on this stack are PascalCase** (`StartRenderProcess`, `OpenView`, `AttachSurface`), matching Chromium. `Smt_*` ABI stays as today. Launch helpers are `RendererProcessHost::Init` and `GpuProcessHost::Init`.

### Mojo polarity

```
Browser:  Remote<MapWidget>  + Receiver<MapWidgetHost>   // renderer
          Remote<Gpu>        + Receiver<GpuHost>         // gpu
Renderer: Receiver<MapWidget> + Remote<MapWidgetHost>
          Remote<Gpu>                                   // paint submit
GPU:      Receiver<Gpu>      + Remote<GpuHost>
```

## Mojom surface (`src/content/public/mojom/map_widget.mojom`)

`module content.mojom;`

Shared types: `ViewKind`, `PresentMode`, `Extent2`, `PointerEvent`, `FeatureId`, `GpuCaps`, `FramePixels` (`generation`, `width_px`, `height_px`, `format`, `handle pixels`).

**MapWidget** (browser → renderer): `Hello() => (RendererCaps)`, `OpenView(ViewKind) => (uint32 view_id)` (**id assigned in renderer**), `CloseView`, `SetExtent`, `SetSelection`, `LegendQuery`, `CatalogOp(string json)`, `ActivateTool`, `DispatchPointer`, `DispatchText`, `PluginCall`, `PrintRequest`, `Shutdown`.

**MapWidgetHost** (renderer → browser): `ViewReady`, `ExtentChanged`, `SelectionChanged`, `LegendSnapshot`, `CatalogDelta`, `PluginEvent`, `ViewCursor`, `ContextMenu`, `RendererDied`.

`AttachSurface` / `ResizeSurface` / `FrameReady` / `ResetGpu` live on **`gpu.mojom`**, not on `MapWidget`. `kIoCall` is not on either pipe.

Fire-and-forget: extent and pointer. Reply + 15s timeout: `Hello`. Reply + 30s: `OpenView`. Timeouts must not block the UI thread (bindings on the IO thread, replies posted to the UI runner).

## GPU process (2D + 3D)

Standalone `--type=gpu` is **required** (Chromium Windows). It is the only process allowed to create a GL or D3D11 device.

| `ViewKind` | GPU backend (v1) | Not |
| --- | --- | --- |
| `kMapEdit` / `kMapData` | `SmtRender` + `SmtGLRenderDevice` (GDI = `kSoftwareDib`) | D3DX9 |
| `kScene3d` | `render/render3d` + `scene3d` / `terrain` / `pointcloud` | `render/d3d` D3DX path |

One GPU device, **N surfaces**, mixed 2D and 3D views in the same process (Chromium: one GPU process, many contexts). Browser `MapWidgetHostView` only **opens** the shared handle; it does not draw the map.

`src/content/public/mojom/gpu.mojom` (`module gpu.mojom;`):

- **Gpu** (browser or renderer → GPU): `Hello() => (GpuCaps)` (`gpu` string, `dxgi_shared`, `has_gl`, `has_d3d11`, `has_3d`), `CreateSurface(view_id, ViewKind, PresentMode, handle? parent_hwnd)`, `Resize(view_id, w, h, dpi)`, `SetVisible`, `Submit2d(view_id, …)` / `Submit3d(view_id, …)` (renderer frame), `ResetDevice`, `LostContextAck`.
- **GpuHost** (GPU → browser): `FrameReady(view_id, FramePixels, fence, cursor_hint)`, `ContextLost(reason)`, `PrintPage(handle, json)`.

Renderer never calls `D3D11CreateDevice`. After `SmtIATool` mutates the map, it `Submit2d`/`Submit3d` to GPU. TDR: GPU process dies or `ContextLost` → `GpuProcessHost` relaunches `--type=gpu`; **renderer stays**; browser drops handles and waits for new `FrameReady`. `ResetGpu()` on the public API maps to `Gpu.ResetDevice` or kill/relaunch GPU only.

Hidden HWND for `SmtRenderDevice::Init` exists **only** in the GPU process.

## Invitation

1. Every process: `ContentMain` → `mojo::core::Init`, IO `base::Thread` (`MessagePumpType::IO`), `ScopedIPCSupport`.
2. Browser: two `PlatformChannel`s; invitations attach `"renderer"` and `"gpu"`.
3. `CreateProcess` **the same `SmartGis.exe`** twice (`--type=renderer`, `--type=gpu`) with `HANDLE_LIST` inheriting only that child's remote endpoint. `CREATE_NO_WINDOW`. One Job `KILL_ON_JOB_CLOSE` for both children.
4. Each child `Accept` + extract its named pipe. Browser `Hello()` on both `MapWidget` and `Gpu`.
5. Renderer receives a GPU channel (browser-brokered, like Chromium `GpuProcessHost::EstablishGpuChannel`) so it can `Submit2d` / `Submit3d` without the browser marshalling every paint.

Do not pass `--pipe=` or a sibling `SmartGisRender.exe` path.

Crash: renderer disconnect → `RendererDied` → relaunch renderer, keep GPU if still up. GPU `ContextLost` / death → relaunch **GPU only**, renderer stays, drop `SharedSurface` handles. Uncommitted SDE edits in a dead **renderer** are lost (multiprocess doc §0.8).

`--in-process-gpu` / `--in-process-renderer`: dev only, default off.

## UI events

Map pointers are taken on the **UI-process native viewport**, then forwarded by `MapWidgetHost`. Same polarity as Chromium: `RenderWidgetHostView` receives HWND/Aura events → `RenderWidgetHost::ForwardMouseEvent` → mojom `Widget`. Ribbon, tree, dialogs, and accelerators are consumed in chrome. Events that miss the map never call `DispatchPointer`. WebView JS must not see map `mousemove`.

### Pipeline

```
User → chrome (ribbon/tree/dialog hit-test) → stop
     → MapWidgetHostView::OnNativeEvent
          DIP → physical pixels; origin = map HWND client origin
          WM_LBUTTONDOWN: SetCapture on the UI HWND (not the GPU process)
     → MapWidgetHost::Forward*
          coalesce queued MouseMove (keep latest + button state)
          Down / Up / Wheel / Key are never coalesced
     → MapWidget.DispatchPointer(view_id, PointerEvent)   // fire-and-forget
     → renderer SmtIATool::MouseMove / LButtonDown / …
     → renderer Gpu.Submit2d|Submit3d
     ← GpuHost.FrameReady(handle)   // GPU process
     ← MapWidgetHost.ViewCursor / ExtentChanged / ContextMenu
```

`ActivateTool` is a command, not an event. Later pointers go to the active `SmtIATool`.

Coordinates are **surface physical pixels**. Chrome converts DIP×DPI in the view; GPU does not assume 96 DPI.

### Input classes

| Kind | v1 |
| --- | --- |
| Mouse / wheel | Map HWND `WM_MOUSE*` or WinUI `PointerRoutedEvent` → `Forward*` |
| Touch | Map to mouse down/move/up. No pinch recognizer in GPU. If needed, UI synthesizes `kWheel` (or a later `kGesture`) |
| Keyboard | Only while the map view has focus. Chrome keeps global accelerators (Ctrl+S) |
| IME | Composition stays in UI (`Imm*` / WinUI `InputPane` / WebView2). Commit only: `DispatchText(view_id, string)` |
| Context menu | **Renderer** sends `ContextMenu`; chrome draws it. GPU does not pop menus |
| DPI / monitor | `ResizeSurface`; following pointers carry the new `dpi` |
| Drag out of HWND | UI `SetCapture`; coordinates may leave the client rect |

Do not wait for a `DispatchPointer` ack before present. Smoothness wins; `generation` drops stale frames. The GPU process does not create a visible input HWND (hidden HWND is only for `SmtRenderDevice::Init` **in GpuMain**).

### Shell adapters

Each chrome only implements `NativeInputTraits<NativeEvent>` (`MSG` / `PointerRoutedEventArgs` / `ui::views::Event`) → `content::InputEvent`. Hosts still must not `#include` mojo; they call PascalCase methods on `MapWidgetHostView` / `MapContents`.

- WebView2: sibling map HWND. Drag-from-web-to-map is `IDropTarget`, not `DispatchPointer`.
- WinUI: convert DIP to physical pixels in the view; no C# hop.
- Views: map child `OnMousePressed` forwards directly.

## Traits (reduce copies)

```cpp
enum class ProcessRole { kBrowser, kRenderer, kGpu };
```

`MapMojoTraits` covers Browser/Renderer `MapWidget` polarity. `GpuMojoTraits` is the same pattern for `Gpu` / `GpuHost` (browser implements `GpuHost`, GPU implements `Gpu`; renderer holds `Remote<Gpu>` only).

**Typemaps** (`StructTraits` / `EnumTraits`): `Extent2`, `PointerEvent` ↔ `content::InputEvent`, `FeatureId`, `FramePixels` ↔ `content::SharedSurface` (`handle` → `HANDLE`, never `uint64` on the Mojo path), `PresentMode`, `ViewKind`. Catalog stays `string` JSON in v1.

**PresentBackendTraits<PresentMode>**: `kSharedTexture` (DXGI NT), `kSoftwareDib` (section), `kChildHwnd` (parent HWND only). Instantiated **only** in `--type=gpu`. `ViewKind` selects 2D vs 3D submit traits (`Submit2d` / `Submit3d`), not a second process.

**NativeInputTraits<NativeEvent>**: Win32 `MSG`, WinUI pointer args, and Views events map to `content::InputEvent`. Hosts do not each copy a `dispatch_mouse` switch.

**Allowed TMP:** concepts, `requires`, `if constexpr`, fold, explicit specialization, CRTP.  
**Not on the main path:** Boost.Hana, C++26 pack indexing, deep recursive constexpr, a second IDL. Escape-hatch named pipe does not get this traits layer.

## Generator / pin

- Slim `build/mojom.gni`: `cpp_only`, invoke Chromium `mojom_bindings_generator.py` with `--use_bundled_pylibs`. Output under `out/gen/content/public/mojom/`.
- Do not import Chromium's 2000-line `mojom.gni` (Blink/Java/typemap flags).
- Generator needs a real Python (not the Store stub). Fail with a readable error.
- `third_party/chromium` is a **pinned sparse** checkout: `base`, `mojo/public`, `mojo/core`, `third_party/abseil-cpp`, `third_party/jinja2`, `third_party/markupsafe`, plus whatever those targets require to link. Include dir `//third_party/chromium` is **not** a `public_config` of `src/base` or `src_all`.
- Collision: Chromium `"base/...."` vs `"base/api.h"` from `//src`. Mojo-compiling targets must not list `//src` as an include root except for `"content/public/..."` via a narrow config.

## Testing / success

| Check | Evidence |
| --- | --- |
| Language | `build.bat` (`//:all`) green at `cc_std=c++23` |
| Generate | `out/gen/.../map_widget.mojom.h` exists |
| Handshake | two invitations: renderer `Hello` + GPU `GpuCaps` within 15s |
| Handle | `GpuHost.FrameReady.pixels` is a Mojo handle from `--type=gpu` |
| 2D+3D | `OpenView(kMapEdit)` and `OpenView(kScene3d)` both present from the **same** GPU process |
| Input | Map HWND mouse/wheel reaches **renderer** `SmtIATool`; ribbon hit does not. IME commit is `DispatchText` only |
| Rebind GPU | `TerminateProcess` GPU → new `--type=gpu` → new handles; renderer stays |
| Rebind renderer | `TerminateProcess` renderer → new `--type=renderer`; GPU can stay |
| Isolation | `SmtCore` compile lines do not use `//third_party/chromium` includes |
| Escape | missing pin: named pipe `Hello` still works; never both transports |

`out\SmartGis.exe --type=gpu --self-test` must create a D3D or GL device and a shared handle **without** a browser HWND. `--type=renderer --self-test` must not create a GPU device.

`content/public` C++ that chrome includes stays free of `mojo::` and Chromium `base`.

## Docs to update in the same implementation change

- `docs/build/ui-shell-multiprocess.md` §0.2 / §0.4 — Browser + Renderer + **standalone GPU**; 2D and 3D in `--type=gpu`; one `SmartGis.exe`.
- `docs/build/src-layout.md` — `src/base/ipc`, `content` Chromium-style type names, C++23, `ContentMain`.
- Root `README.md` — C++23; `build.bat render` → `SmartGis.exe --type=`; refresh **最后更新**.
- `build/README.md` — `cc_std` default.
- `docs/README.md` — link this spec.
- Hosts under `src/app/{webview2,winui,views}` — `MapContents` / PascalCase; browser-only `--ui=`.

## Risks

- Chromium `base` + abseil size and GN port: pin a known-good revision; wrap with our GN, do not take Chromium `BUILDCONFIG`.
- WinUI C++/WinRT after dropping `/std:c++17`: fix projections or isolate that target only.
- Legacy MFC / 2010 sources under C++23: fix errors, do not weaken the standard.
- Mojom generator Python / jinja pin drift vs `mojo/core`.
- TDR vs Job: GPU restart must not kill renderer; Job may need two groups or `KILL_ON_JOB_CLOSE` only for browser exit, not GPU crash.
- GL + D3D11 in one GPU process (2D vs 3D): share DXGI device / GL-D3D interop; do not spawn a second GPU process.
- Handle inheritance vs antivirus: keep `HANDLE_LIST`; fallback `NamedPlatformChannel` only if inherit is blocked, still Mojo.

## Success

- Repo compiles as C++23 (`build.bat`).
- Chrome `MapContents::Create()` starts `SmartGis.exe --type=renderer` **and** `--type=gpu`.
- No `SmartGisRender.exe` on the product path.
- 2D (`kMapEdit`) and 3D (`kScene3d`) frames both come from the GPU process `FrameReady` handle.
- Killing GPU relaunches `--type=gpu` only; killing renderer relaunches `--type=renderer`.
- 31 DLLs still have no Chromium `base` on their include path.
