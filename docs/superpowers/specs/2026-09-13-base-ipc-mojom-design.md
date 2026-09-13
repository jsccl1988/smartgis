<!--

Copyright (c) 2026 The Mogu Authors.

All rights reserved.

-->



# Base IPC + named pipe / pickle (OOP map)



**Date:** 2026-09-13  

**Status:** accepted  

**Scope:** one implementation plan. Move host transport out of `content/common/ipc.h` into `src/base/ipc`, use **Win32 named pipe + mogu BinarySink/pickle** (C++ structs with `archive()`), name the multiprocess map stack like Chromium `WebContents` / `RenderProcessHost` / `Widget`, and use **one PE with Chromium-style `--type=` entry points** (no `SmartGisRender.exe`). **No Chromium. No protobuf. No mojom generator.**



## Goal



Windows OOP map rendering uses **product IPC**: Win32 named pipe + mogu **BinarySink/pickle** wire. C++ message structs implement `archive()` for encode/decode. A **standalone GPU process** (`--type=gpu`) owns all **2D and 3D** map painting (GL / D3D11 / scene3d). The browser only presents. Same PE: `content::ContentMain`, children relaunch `SmartGis.exe` with `--type=`. Language floor is **C++23 for the whole tree**.



## Non-goals



- Do not vendor Blink, Chromium `content/`, viz, Chromium `ui/views`, or Skia wholesale.

- Do not add Chromium Mojo, mojom, or a sparse `chromium/chromium` pin.

- Do not add protobuf or any second IDL / code generator for IPC.

- Do not ship a second map image (`SmartGisRender.exe`). Children are `SmartGis.exe --type=…`.

- Do not run `SmtRenderDevice`, GL, D3D11, or 3D engines in the browser process.

- Do not put 2D and 3D on different GPU processes (one GPU process, N surfaces, mixed `ViewKind`).

- Do not replace present with Chromium viz / command buffer.

- Do not put `kIoCall` on the map widget pipe (that is `--type=utility` in v1.5).

- Qt is banned.



## Language (whole repo)



Windows `declare_args` `cc_std` is **`c++23`**, same as the non-Windows branch in `build/BUILDCONFIG.gn`. `c_std` stays `c17` on Windows unless a separate change needs C23.



- Legacy `Smt_*` TUs compile as C++23; fix conformance errors in place. Do not leave a second `cc_std` for `smt_shared_library`.

- Traits in this spec may use C++20 concepts, `requires`, `if constexpr`, and C++23 that MSVC v145 accepts.



## Architecture



One product image, `ContentMain` dispatch (Chromium-style switch names only — **not** Chromium IPC):



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



`--ui=views|web|winui` is **browser-only**. Child processes never load WebView2, WinUI, MFC, or Views chrome.



`RendererProcessHost::Launch` / `GpuProcessHost::Launch` use `GetModuleFileNameW(nullptr)`, copy the browser command line, set `--type=renderer` or `--type=gpu`, append `--pipe=` (see Invitation). Never a different `output_name`. Product topology is **always two children**. `--in-process-gpu` is debug-only and **default off**.



## Single binary / ContentMain



```

wWinMain → content::ContentMain(ContentMainParams)

             CommandLine --type=

             kBrowser   → BrowserMain

             kRenderer  → RendererMain

             kGpu       → GpuMain

             kUtility   → UtilityMain

```



Switch names match Chromium process labels: `--type=renderer`, `--type=gpu`, `--type=utility`. Browser is the default (omit `--type` or `--type=browser`).



| Label | This repo |

| --- | --- |

| `SmartGis.exe` | `out/SmartGis.exe` |

| `--type=renderer` | `SmtMap` + `SmtIATool` only. **No** D3D/GL device |

| `--type=gpu` | **Required** child. One GPU device paints **2D and 3D** |

| `--type=utility` | future IO/SDE payload |

| `--in-process-gpu` | GPU Main inside renderer. **Default off.** Debug / CI only |

| `--in-process-renderer` | all Mains in the browser (dev only, default off) |



Default launch: Browser starts **Renderer and GPU**. Present and `Smt*Render*` / `scene3d` / `terrain` / `pointcloud` run only in `GpuMain`.



GN: one `executable("smartgis")` links browser + renderer + gpu + utility mains. `src_all` stays 31 DLLs and does not link this exe. `build.bat render` becomes `out\SmartGis.exe --type=renderer --self-test` (and `--type=gpu --self-test`). Retire `//src/gpu:gpu` as a separate `console_app`.



**Product wire:** Win32 named pipe `\\.\pipe\smartgis-host-<browser-pid>` (or per-session name) + length-prefixed frames. Payloads are **pickle BinarySink** blobs produced/consumed by C++ structs with `archive()`. This is the **only** v1 transport — not an escape hatch for a future Mojo path.



## Components



| Unit | Role | Depends on |

| --- | --- | --- |

| `base::ipc` (`src/base/ipc`) | Named pipe server/client, frame envelope, BinarySink encode/decode | mogu-style pickle helpers (in-tree) |

| `content::MapContents` | Public session API (today `MapSession`) | `RendererProcessHost` |

| `MapContentsObserver` | Frame / extent / death callbacks | none (chrome implements) |

| `MapWidgetHostView` | Public viewport; `latest()` shared surface | `MapWidgetHost` |

| `content::ContentMain` | `wWinMain` dispatch on `--type=` | app + content |

| `RendererProcessHost` | Launch `SmartGis.exe --type=renderer`, Job, `--pipe=` | `base::ipc` |

| `GpuProcessHost` | Always launch `--type=gpu`; TDR restarts this process only | `base::ipc` |

| `MapWidgetHost` | Per-view proxy; `Forward*` input to **renderer** | pipe messages |

| `gpu::GpuMain` | `--type=gpu` entry; 2D+3D backends | `src/gpu`, `src/render/*` |



`MapRenderProcessHost` is an alias for `RendererProcessHost` during the rename; do not keep both in public headers.



Public headers stay under `src/content/public/` (no `public/browser/` third nest). Includes: `"content/public/map_contents.h"`.



`//src:src_all` does **not** depend on IPC wire internals beyond what chrome needs through `content` public API.



## Chromium naming (public API only)



| Retired | v1 |

| --- | --- |

| `content::MapSession` | `content::MapContents` |

| `MapSessionClient` | `content::MapContentsObserver` |

| `create_map_session()` | `MapContents::Create()` |

| `MapView` | `content::MapWidgetHostView` |

| hidden pipe owner | `content::RendererProcessHost` (`GpuProcessHost` for `--type=gpu`) |

| per `view_id` | `content::MapWidgetHost` |

| `ToolRouter` | methods on `MapWidgetHost` (optional typedef during the move) |



Delete `map_session.h` / `map_view.h` / `tool_router.h` after hosts are updated. No long-lived aliases.



**Method names on this stack are PascalCase** (`StartRenderProcess`, `OpenView`, `AttachSurface`), matching Chromium public API shape. `Smt_*` ABI stays as today.



## Wire format (pickle BinarySink)



**Frame envelope** (all messages):



| Field | Type | 说明 |

| --- | --- | --- |

| `magic` | `u32` | `'SMT1'` |

| `version` | `u16` | 协议主版本 |

| `type` | `u16` | message discriminant |

| `flags` | `u16` | `kJson` / `kBinary` / `kNeedAck` |

| `view_id` | `u32` | `0` = session 级 |

| `payload_bytes` | `u32` | pickle blob length |

| `payload` | bytes | BinarySink output from `struct.archive()` |



**C++ structs** (examples — implement `void archive(Archive& ar)` or project equivalent):



- `Hello` / `HelloAck` — protocol version, GPU caps

- `OpenView` / `ViewReady` — `ViewKind`, `view_id` (**id assigned in renderer**)

- `CloseView`, `SetExtent`, `ExtentChanged`, `SetSelection`, `SelectionChanged`

- `LegendQuery` / `LegendSnapshot`, `CatalogOp` / `CatalogDelta` (JSON string in payload for v1)

- `ActivateTool`, `DispatchPointer`, `DispatchText`

- **`PluginCall`** — `{ plugin_id, method, bytes }` on the **same envelope** (future plugin IPC; no second IDL)

- **`PluginEvent`** — renderer → browser plugin callback

- `AttachSurface`, `ResizeSurface`, `FrameReady`, `ResetGpu` — on GPU session (same pipe family or dedicated GPU pipe name; v1 may multiplex on one pipe with `type`)

- `RenderDied` / `ContextLost`, `PrintRequest` / `PrintPage`



Shared value types in structs: `ViewKind`, `PresentMode`, `Extent2`, `PointerEvent`, `FeatureId`, `GpuCaps`, `FramePixels` (`generation`, dimensions, `format`, platform `HANDLE` for pixels — serialized as inheritable handle token on wire).



Fire-and-forget: extent and pointer. Reply + 15s timeout: `Hello`. Reply + 30s: `OpenView`. Timeouts must not block the UI thread (IO thread decode, replies posted to UI runner).



Do **not** run mojom, protobuf, or a parallel JSON-only protocol for control messages.



## GPU process (2D + 3D)



Standalone `--type=gpu` is **required**. It is the only process allowed to create a GL or D3D11 device.



| `ViewKind` | GPU backend (v1) | Not |

| --- | --- | --- |

| `kMapEdit` / `kMapData` | `SmtRender` + `SmtGLRenderDevice` (GDI = `kSoftwareDib`) | D3DX9 |

| `kScene3d` | `render/render3d` + `scene3d` / `terrain` / `pointcloud` | D3D9 path removed |



One GPU device, **N surfaces**, mixed 2D and 3D views in the same process. Browser `MapWidgetHostView` only **opens** the shared handle; it does not draw the map.



Renderer never calls `D3D11CreateDevice`. After `SmtIATool` mutates the map, it submits frames to GPU via wire messages. TDR: GPU process dies or `ContextLost` → `GpuProcessHost` relaunches `--type=gpu`; **renderer stays**; browser drops handles and waits for new `FrameReady`.



Hidden HWND for `SmtRenderDevice::Init` exists **only** in the GPU process.



## Invitation / child launch



**v1 (current):** no Mojo invitation. Children receive **`--pipe=`** (and `--parent-pid=`, `--session=` as today) on the command line.



1. Browser: `CreateNamedPipeW` (or connect server in child) for `\\.\pipe\smartgis-host-<pid>`.

2. `CreateProcess` **the same `SmartGis.exe`** twice (`--type=renderer`, `--type=gpu`) with `--pipe=` pointing at the pipe name. `CREATE_NO_WINDOW`. One Job `KILL_ON_JOB_CLOSE` for both children.

3. Each child connects, sends `Hello` pickle frame; browser replies `HelloAck`.

4. Renderer may receive a brokered GPU channel (browser forwards handle or second pipe name) so it can submit paint without the browser marshalling every frame.



Do **not** pass `--mojo-platform-channel-handle=`. Do **not** fetch Chromium for invitation.



Later task (not v1): replace `--pipe=` string bootstrap with a richer invitation struct on the same pickle wire — still **no** second IDL.



Crash: renderer disconnect → `RendererDied` → relaunch renderer, keep GPU if still up. GPU `ContextLost` / death → relaunch **GPU only**, renderer stays, drop `SharedSurface` handles.



## UI events



Map pointers are taken on the **UI-process native viewport**, then forwarded by `MapWidgetHost`. Ribbon, tree, dialogs, and accelerators are consumed in chrome. Events that miss the map never call `DispatchPointer`.



### Pipeline



```

User → chrome (ribbon/tree/dialog hit-test) → stop

     → MapWidgetHostView::OnNativeEvent

          DIP → physical pixels; origin = map HWND client origin

          WM_LBUTTONDOWN: SetCapture on the UI HWND (not the GPU process)

     → MapWidgetHost::Forward*

          coalesce queued MouseMove (keep latest + button state)

          Down / Up / Wheel / Key are never coalesced

     → DispatchPointer(view_id, PointerEvent)   // fire-and-forget on pipe

     → renderer SmtIATool::MouseMove / LButtonDown / …

     → renderer Submit2d|Submit3d to GPU

     ← FrameReady(handle)   // GPU process

     ← ViewCursor / ExtentChanged / ContextMenu

```



`ActivateTool` is a command, not an event. Coordinates are **surface physical pixels**.



Hosts must not include wire codecs; they call PascalCase methods on `MapWidgetHostView` / `MapContents`.



## Traits (reduce copies)



```cpp

enum class ProcessRole { kBrowser, kRenderer, kGpu };

```



**Archive traits** for `Extent2`, `PointerEvent` ↔ `content::InputEvent`, `FeatureId`, `FramePixels` ↔ `content::SharedSurface`, `PresentMode`, `ViewKind` — all via `archive()` on structs, not a code generator.



**PresentBackendTraits<PresentMode>**: `kSharedTexture`, `kSoftwareDib`, `kChildHwnd`. Instantiated **only** in `--type=gpu`.



**NativeInputTraits<NativeEvent>**: Win32 `MSG`, WinUI pointer args, Views events → `content::InputEvent`.



**Allowed TMP:** concepts, `requires`, `if constexpr`, explicit specialization.  

**Not on the main path:** a second IDL, protobuf, Mojo.



## Testing / success



| Check | Evidence |

| --- | --- |

| Language | `build.bat` (`//:all`) green at `cc_std=c++23` |

| Wire | `Hello` / `HelloAck` over named pipe within 15s |

| Handle | `FrameReady` carries inheritable pixel handle from `--type=gpu` |

| 2D+3D | `OpenView(kMapEdit)` and `OpenView(kScene3d)` both present from the **same** GPU process |

| Input | Map HWND mouse/wheel reaches **renderer** `SmtIATool`; ribbon hit does not |

| Rebind GPU | Kill GPU process → new `--type=gpu` → new handles; renderer stays |

| Rebind renderer | Kill renderer → new `--type=renderer`; GPU can stay |

| Isolation | `SmtCore` compile lines do not pull IPC wire headers |

| Plugin shape | `PluginCall { plugin_id, method, bytes }` defined on same envelope (may be stub until wired) |



`out\SmartGis.exe --type=gpu --self-test` must create a D3D or GL device and a shared handle **without** a browser HWND. `--type=renderer --self-test` must not create a GPU device.



`content/public` C++ that chrome includes stays free of pipe implementation details.



## Docs to update in the same implementation change



- `docs/build/ui-shell-multiprocess.md` §0.2 / §0.4 — Browser + Renderer + **standalone GPU**; one `SmartGis.exe`; named pipe + pickle.

- `docs/build/src-layout.md` — `src/base/ipc`, `content` Chromium-style type names, C++23, `ContentMain`.

- Root `README.md` — C++23; `build.bat render` → `SmartGis.exe --type=`; refresh **最后更新**.

- `build/README.md` — `cc_std` default.

- `docs/README.md` — link this spec.

- Hosts under `src/app/{winui,views}` — `MapContents` / PascalCase; browser-only `--ui=`.



## Risks



- Pickle wire versioning: bump `version` field; never run two transports in parallel.

- WinUI C++/WinRT under C++23: fix projections or isolate that target only.

- Legacy MFC / 2010 sources under C++23: fix errors, do not weaken the standard.

- TDR vs Job: GPU restart must not kill renderer.

- GL + D3D11 in one GPU process: share DXGI device; do not spawn a second GPU process.

- Handle inheritance vs antivirus: keep `HANDLE_LIST` or duplicate via broker message on pipe.



## Success



- Repo compiles as C++23 (`build.bat`).

- Chrome `MapContents::Create()` starts `SmartGis.exe --type=renderer` **and** `--type=gpu` with `--pipe=`.

- No `SmartGisRender.exe` on the product path.

- 2D and 3D frames both come from the GPU process `FrameReady` handle.

- Killing GPU relaunches `--type=gpu` only; killing renderer relaunches `--type=renderer`.

- 31 DLLs still have no wire codec on their include path.

- No Chromium pin, no mojom, no protobuf in the product IPC path.


