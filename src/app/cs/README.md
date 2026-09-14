<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/cs` — C# WinUI host (sibling chrome)

External C# chrome over `content::MapContents`. Not the Views + Skia
endgame. Product exe: `out/SmartGisCs.exe`. Reusable control:
`SmartGis.WinUI.MapView`.

```bat
build.bat cs
out\SmartGisCs.exe
out\SmartGisCs.exe --self-test
out\sg_host_test.exe
```

Requires .NET 8 SDK (`dotnet` on PATH) and sibling `SmartGisRender.exe`
(the C# PE cannot `--type=gpu`). `build.bat cs` also sets
`smt_build_render=true`.

`smt_build_cs` default false. Not in `group("all")` / `src_all`.

## FFI

`smartgis_host[_d].dll` exports `sg_host_*` (`src/app/cs/native/sg_host.h`).
C# `SmartGis.Host.MapSession` P/Invokes that DLL. Do not Load `SmtGisCore`
into the C# process.

Present: HWND island + software DIB (same as `app/winui/MapHost`).

## IDE layout

Same regions and command ids as Views / WinUI C++ / CEF:

```
MenuBar (File: Open/Exit · View: Map/Data/3D · Tools: Select/Draw/Clear/Pan)
  Catalog (TreeView + Refresh / Add layer) | Map Edit|Data|3D + MapView | Ambox
  Inspector: FeatureInfo | AttributeTable
  StatusBar
```

Embed in another WinUI 3 app:

```csharp
var map = new SmartGis.WinUI.MapView();
map.AttachWindow(WinRT.Interop.WindowNative.GetWindowHandle(window));
```

Copy `smartgis_host_d.dll`, `SmartGis.Host.dll`, `SmartGis.WinUI.dll`, and
`SmartGisRender.exe` next to the foreign exe.

C# only (native DLL already in `out/` or you are iterating chrome):

```bat
dotnet build src\app\cs\SmartGisCs\SmartGisCs.csproj -c Debug -o out
```

## Spec

[`docs/superpowers/specs/2026-09-15-app-cs-winui-host-design.md`](../../../docs/superpowers/specs/2026-09-15-app-cs-winui-host-design.md)
