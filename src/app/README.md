<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app` — product hosts

Endgame / prototype shells only:

| Path | Binary | Notes |
| --- | --- | --- |
| `views/` | `SmartGisViews.exe` | Destination chrome (`build.bat views`) |
| `winui/` | `SmartGisWinui.exe` | Non-endgame prototype (`build.bat winui`) |

MFC `SmartGis.exe` and `app_core` live in [`../legacy_app/`](../legacy_app/). Do not reintroduce MFC frame/`CView` sources here.
