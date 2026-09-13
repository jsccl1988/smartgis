<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Base IPC + Chromium Mojo / mojom (OOP map)

**Date:** 2026-09-13  
**Status:** draft (awaiting review)  
**Scope:** one implementation plan. Move host transport out of `content/common/ipc.h` into `src/base/ipc`, adopt Chromium Mojo (static embedder + **mojom generator in v1**), and name the multiprocess map stack like Chromium `WebContents` / `RenderProcessHost` / `Widget`.

## Goal

Windows OOP map rendering uses **real Chromium Mojo**: invitation, message pipes, first-class `handle` transfer (DXGI NT handle / DIB section), and **generated C++ bindings from `.mojom`**. Chrome processes talk only to `content::MapContents`. Pixel present stays in `src/gpu` (not viz). Language floor is **C++23 for the whole tree**.

## Non-goals

- Do not vendor Blink, `content/` from Chromium, `gpu/` viz, `ui/views` from Chromium, or Skia wholesale.
- Do not put Chromium `base/` on the include path of `SmtCore` / `src_all` 31 DLLs.
- Do not replace `SmartGisRender.exe` with Chromium's GPU command buffer.
- Do not put `kIoCall` on the map widget pipe (that is a second invitation pipe in v1.5).
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

```
chrome exe (Web / WinUI / Views / leftover MFC)
  MapContents                    // public, like WebContents
  MapContentsObserver
  MapWidgetHostView             // present HWND / swap chain
       │  does not include mojo headers
       v
content (internal)
  MapRenderProcessHost          // invitation + Job + child
  MapWidgetHost                 // per surface, Forward* input
  MapMojoPipe<kBrowser>
       │  generated content.mojom.MapWidget / MapWidgetHost
       v
third_party/chromium (pin)
  base + mojo/core + mojo/public + mojom tools + abseil + jinja2
       │  static link into chrome exe + SmartGisRender.exe only
       v
SmartGisRender.exe
  MapMojoPipe<kRenderer>
  gpu::MapWidget                // implements MapWidget
  PresentBackendTraits<>        // DXGI / DIB / child HWND
```

Bootstrap is Chromium `PlatformChannel` + `OutgoingInvitation` / `IncomingInvitation`, not `CreateNamedPipe(\\.\pipe\smartgis-host-<pid>)` polling. Command line: `--parent-pid` `--session` `--mojo-platform-channel-handle=<int>`. Drop `--pipe=` on the Mojo path.

Same-revision parent and child **statically** link `mojo::core::Init` + IO thread + `ScopedIPCSupport`. Dynamic `mojo_core.dll` is not v1 (bindings already require Chromium `base` in the exe).

Escape hatch: if the Chromium pin is missing, freeze today's named pipe + `HostMsg` and do not run it at the same time as an invitation. New fields are added only in `.mojom`.

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `base::ipc` (`src/base/ipc`) | Invitation / message-pipe / platform-handle facade; Chromium include dirs **private** | `//third_party/chromium` (ipc targets only) |
| `content::MapContents` | Public session API (today `MapSession`) | `MapRenderProcessHost` |
| `MapContentsObserver` | Frame / extent / death callbacks | none (chrome implements) |
| `MapWidgetHostView` | Public viewport; `latest()` shared surface | `MapWidgetHost` |
| `MapRenderProcessHost` | `CreateProcess`, Job `KILL_ON_JOB_CLOSE`, invitation | `base::ipc` |
| `MapWidgetHost` | Per-view proxy; `ForwardMouseEvent` / `ForwardWheelEvent` / `ForwardKeyboardEvent` | `Remote<MapWidget>` |
| `content.mojom` | IDL + generated C++ | slim `build/mojom.gni` |
| `gpu::MapWidget` | Child implementation of `MapWidget` | present traits + Smt adapter |

Public headers stay under `src/content/public/` (no `public/browser/` third nest). Includes look Chromium-like: `"content/public/map_contents.h"`.

`//src:src_all` does **not** depend on Chromium `base` or mojom generation. `map_contents` implementation and `gpu` link Chromium only when `smt_build_render` / chrome exes are on.

## Chromium naming

| Retired | v1 |
| --- | --- |
| `content::MapSession` | `content::MapContents` |
| `MapSessionClient` | `content::MapContentsObserver` |
| `create_map_session()` | `MapContents::Create()` |
| `MapView` | `content::MapWidgetHostView` |
| hidden pipe owner | `content::MapRenderProcessHost` |
| per `view_id` | `content::MapWidgetHost` |
| `ToolRouter` | methods on `MapWidgetHost` (optional typedef during the move) |
| mojom `MapHost` (UI→R) | `content.mojom.MapWidget` |
| mojom `MapClient` (R→UI) | `content.mojom.MapWidgetHost` |
| `gpu` surface slot | `gpu::MapWidget` |

Delete `map_session.h` / `map_view.h` / `tool_router.h` after hosts are updated. No long-lived aliases.

**Method names on this stack are PascalCase** (`StartRenderProcess`, `OpenView`, `AttachSurface`), matching Chromium. `Smt_*` ABI stays as today.

### Mojo polarity

```
UI:   Remote<MapWidget>     + Receiver<MapWidgetHost>
GPU:  Receiver<MapWidget>   + Remote<MapWidgetHost>
```

## Mojom surface (`src/content/public/mojom/map_widget.mojom`)

`module content.mojom;`

Shared types: `ViewKind`, `PresentMode`, `Extent2`, `PointerEvent`, `FeatureId`, `GpuCaps`, `FramePixels` (`generation`, `width_px`, `height_px`, `format`, `handle pixels`).

**MapWidget** (browser → renderer): `Hello() => (GpuCaps)`, `OpenView(ViewKind) => (uint32 view_id)` (**id assigned in GPU**), `CloseView`, `AttachSurface(view_id, PresentMode, handle? parent_hwnd)`, `ResizeSurface`, `SetVisible`, `SetExtent`, `SetSelection`, `LegendQuery`, `CatalogOp(string json)`, `ActivateTool`, `DispatchPointer`, `DispatchText`, `PluginCall`, `PrintRequest`, `ResetGpu`, `Shutdown`.

**MapWidgetHost** (renderer → browser): `ViewReady`, `FrameReady(view_id, FramePixels, uint64 fence, uint32 cursor_hint)` (**absorbs old `kSharedHandle`**), `ExtentChanged`, `SelectionChanged`, `LegendSnapshot`, `CatalogDelta`, `PluginEvent`, `PrintPage(handle page_dib, string meta_json)`, `ViewCursor`, `ContextMenu`, `RenderDied`.

`HelloAck` is the `Hello()` reply. `kIoCall` is not on this pipe.

Fire-and-forget: extent and pointer. Reply + 15s timeout: `Hello`. Reply + 30s: `OpenView`. Timeouts must not block the UI thread (bindings on the IO thread, replies posted to the UI runner).

## Invitation

1. Both processes: `mojo::core::Init`, IO `base::Thread` (`MessagePumpType::IO`), `ScopedIPCSupport`.
2. UI: `PlatformChannel`; `OutgoingInvitation::AttachMessagePipe("host")`.
3. `CreateProcess` `SmartGisRender.exe` with `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` inheriting **only** the remote channel handle. `CREATE_NO_WINDOW`. Assign Job `KILL_ON_JOB_CLOSE`.
4. `OutgoingInvitation::Send`. Child `IncomingInvitation::Accept` + `ExtractMessagePipe("host")`.
5. Bind `MapMojoPipe<Role>`. UI calls `Hello()`.

Crash: disconnect ⇒ `MapContentsObserver` death notification ⇒ drop all `view_id` and handles, keep extent / selection / catalog JSON / view kinds ⇒ new invitation ⇒ `OpenView` each view. Uncommitted SDE edits in the dead process are lost (same as multiprocess doc §0.8).

`ResetGpu()` rebuilds the D3D device in-process; next `FrameReady` carries a new handle and a higher `generation`.

`--in-process-render` (dev): in-process message pipe, same interfaces, default off.

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
     → gpu::MapWidget → SmtIATool::MouseMove / LButtonDown / …
     ← FrameReady / ViewCursor / ExtentChanged / ContextMenu
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
| Context menu | GPU sends `ContextMenu(x, y, json)`; **chrome draws the menu**. No `TrackPopupMenu` in render except a documented whitelist |
| DPI / monitor | `ResizeSurface`; following pointers carry the new `dpi` |
| Drag out of HWND | UI `SetCapture`; coordinates may leave the client rect |

Do not wait for a `DispatchPointer` ack before present. Smoothness wins; `generation` drops stale frames. The GPU process does not create a visible input HWND (hidden HWND is only for `SmtRenderDevice::Init`).

### Shell adapters

Each chrome only implements `NativeInputTraits<NativeEvent>` (`MSG` / `PointerRoutedEventArgs` / `ui::views::Event`) → `content::InputEvent`. Hosts still must not `#include` mojo; they call PascalCase methods on `MapWidgetHostView` / `MapContents`.

- WebView2: sibling map HWND. Drag-from-web-to-map is `IDropTarget`, not `DispatchPointer`.
- WinUI: convert DIP to physical pixels in the view; no C# hop.
- Views: map child `OnMousePressed` forwards directly.

## Traits (reduce copies)

```cpp
enum class ProcessRole { kBrowser, kRenderer };

template <ProcessRole R>
struct MapMojoTraits;

template <>
struct MapMojoTraits<ProcessRole::kBrowser> {
  using Widget = mojo::Remote<mojom::MapWidget>;
  using WidgetHost = mojo::Receiver<mojom::MapWidgetHost>;
};

template <>
struct MapMojoTraits<ProcessRole::kRenderer> {
  using Widget = mojo::Receiver<mojom::MapWidget>;
  using WidgetHost = mojo::Remote<mojom::MapWidgetHost>;
};
```

One `MapMojoPipe<R>::Bind(ScopedMessagePipeHandle)` for both ends. Invitation send vs accept is `InvitationTraits<kOutgoing>` / `kIncoming`, not two hand-copied files.

**Typemaps** (`StructTraits` / `EnumTraits`): `Extent2`, `PointerEvent` ↔ `content::InputEvent`, `FeatureId`, `FramePixels` ↔ `content::SharedSurface` (`handle` → `HANDLE`, never `uint64` on the Mojo path), `PresentMode`, `ViewKind`. Catalog stays `string` JSON in v1.

**PresentBackendTraits<PresentMode>**: `kSharedTexture` (DXGI NT), `kSoftwareDib` (section), `kChildHwnd` (parent HWND only). `gpu::MapWidget::AttachSurface` and `MapWidgetHostView` open paths share the traits.

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
| Handshake | invitation + `Hello()` / `GpuCaps` within 15s |
| Handle | `FrameReady.pixels` is a Mojo handle; DXGI `OpenSharedResource1` or DIB `MapViewOfFile` |
| Input | Map HWND mouse/wheel reaches GPU `SmtIATool`; ribbon hit does not. IME commit is `DispatchText` only |
| Rebind | `TerminateProcess` render → new invitation → new `view_id` + a frame |
| Isolation | `SmtCore` compile lines do not use `//third_party/chromium` includes |
| Escape | missing pin: named pipe `Hello` still works; never both transports |

`out/SmartGisRender.exe --self-test` becomes invitation-based (or a tiny test host + render). `build.bat e2e` may skip render until the pin is in tree; then it must pass.

`content/public` C++ that chrome includes stays free of `mojo::` and Chromium `base`.

## Docs to update in the same implementation change

- `docs/build/ui-shell-multiprocess.md` §0.4 — Mojo invitation + mojom names; pipe-by-name is escape hatch.
- `docs/build/src-layout.md` — `src/base/ipc`, `content` Chromium-style type names, C++23.
- Root `README.md` — C++23; refresh **最后更新**.
- `build/README.md` — `cc_std` default.
- `docs/README.md` — link this spec.
- Hosts under `src/app/{webview2,winui,views}` — `MapContents` / PascalCase.

## Risks

- Chromium `base` + abseil size and GN port: pin a known-good revision; wrap with our GN, do not take Chromium `BUILDCONFIG`.
- WinUI C++/WinRT after dropping `/std:c++17`: fix projections or isolate that target only.
- Legacy MFC / 2010 sources under C++23: fix errors, do not weaken the standard.
- Mojom generator Python / jinja pin drift vs `mojo/core`.
- Handle inheritance vs antivirus: keep `HANDLE_LIST`; fallback `NamedPlatformChannel` only if inherit is blocked, still Mojo, not the old `HostMsg` pipe.

## Success

- Repo compiles as C++23 (`build.bat`).
- Chrome `MapContents::Create()` starts `SmartGisRender.exe` via Mojo invitation.
- A map surface presents via `FrameReady` Mojo `handle`.
- Killing render restarts and reattaches.
- 31 DLLs still have no Chromium `base` on their include path.
