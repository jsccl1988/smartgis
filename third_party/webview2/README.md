<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# WebView2 (report browser backend)

Pinned for **local HTML report** embedding only — not a product chrome shell.
CEF product-shell remains rejected; a future `CefReportBrowser` may implement the same `plugin::ReportBrowser` seam.

## Layout

| Path | Role |
| --- | --- |
| `include/WebView2.h` (+ EnvironmentOptions) | Headers (vendored from NuGet) |
| `bin/x64/WebView2Loader.dll` (+ `.lib`) | Loader (vendored; refresh from NuGet when bumping) |

**NuGet pin:** `Microsoft.Web.WebView2` **1.0.2903.40**

Refresh:

```bat
nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory %TEMP%\wv2
copy /Y %TEMP%\wv2\Microsoft.Web.WebView2.1.0.2903.40\build\native\include\*.h include\
copy /Y %TEMP%\wv2\Microsoft.Web.WebView2.1.0.2903.40\build\native\x64\WebView2Loader.dll bin\x64\
copy /Y %TEMP%\wv2\Microsoft.Web.WebView2.1.0.2903.40\build\native\x64\WebView2Loader.dll.lib bin\x64\
```

Runtime: Evergreen **WebView2 Runtime** on the machine (or Fixed Version separately). Missing Runtime → `ReportBrowser` returns unavailable; shell stays up.

## GN

`//third_party/webview2:webview2` — headers + link loader. Consumers: `plugin/runtime/browser`.
