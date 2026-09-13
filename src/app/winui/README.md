<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Scheme 2 chrome (`//src/app/winui`)

Unpackaged Win32 + Windows App SDK bootstrap. Product exe: `out/SmartGisWinui.exe`.

```bat
build.bat winui
```

Requires `smt_build_winui=true` (set by that alias). Not in `group("all")`.

## WinAppSDK

Pinned NuGet: **Microsoft.WindowsAppSDK 1.7.260224002** extracted at:

`third_party/windows_app_sdk/Microsoft.WindowsAppSDK.1.7.260224002/include/MddBootstrap.h`

Do not use the 2.x meta-package (no headers). C++/WinRT projections are generated into `out/winui_winrt` by `gen_winrt.bat` (Windows SDK `cppwinrt.exe`).

## Map host

- **SwapChainPanel** when `content::MapView::latest()` has a DXGI NT handle.
- Else **HWND island** (child HWND over the map slot).
- OOP: `CreateProcess(SmartGisRender.exe)` when that image sits next to the exe.
- Else marked **LoadLibrary** probe of `Smt*` DLLs (no `Init` in chrome).

Host ABI: `#include` `src/content/public` when present; otherwise local `content::MapSession` / `MapView` in `detail/`.
