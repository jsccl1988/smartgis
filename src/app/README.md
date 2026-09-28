<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app` — product host

Desktop shell is Views only.

| Path | Binary | Notes |
| --- | --- | --- |
| `views/` | `SmartGisViews.exe` | Product shell (`build.bat app` / `build.bat views`) |

Product contract: MenuBar · Catalog · Ambox · Map Edit\|Data\|3D · Inspector
(FeatureInfo / Attribute) · StatusBar. Map present stays on `content` / GPU.

MFC `SmartGis.exe` and `app_core` live in [`../legacy/app/`](../legacy/app/).
Do not reintroduce MFC frame/`CView` sources here, or CEF / WinUI / C# hosts.
