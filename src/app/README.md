<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app` — product hosts

Endgame / prototype shells only (three interchangeable product chromes):

| Path | Binary | Notes |
| --- | --- | --- |
| `views/` | `SmartGisViews.exe` | Destination chrome (`build.bat views`) |
| `winui/` | `SmartGisWinui.exe` | Non-endgame prototype (`build.bat winui`) |
| `cef/` | `SmartGisCef.exe` | CEF HTML chrome + HWND map slots (`build.bat cef`) |
| `cs/` | `SmartGisCs.exe` | C# WinUI 3 host + `MapView` control (`build.bat cs`) |

Product contract (all four): MenuBar · Catalog · Ambox · Map Edit|Data|3D ·
Inspector (FeatureInfo / Attribute) · StatusBar. Toolkit differs; regions and
command ids align. Map present stays on `content` / GPU (CEF and C# use
`SmartGisRender.exe` escape hatch).

MFC `SmartGis.exe` and `app_core` live in [`../legacy/app/`](../legacy/app/).
Do not reintroduce MFC frame/`CView` sources here.
