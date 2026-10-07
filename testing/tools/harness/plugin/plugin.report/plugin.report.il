// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Report dock integration (WebView2 / fake bridge).
// IR: horizon HWND → plugin report sample → inspector tab.

script "plugin.report" {
  seq @inproc {
    // --- horizon ---
    mark("show");
    pump(ms=400);
    require_hwnd();
    mark("hwnd-ok");
    suppress_dialogs(on=1);

    // --- plugin ---
    require_plugins();
    resolve_data(kind="plugin", leaf="report/sample", as="rep");
    require_var(name="rep", fail_mark="report-sample-missing");
    // Open via FakeReportBrowser (--report-browser=fake) before showing the
    // Report inspector tab — WebView2 init on that tab can abort (exit 3).
    open_report(path="$rep");
    mark("report-ok");
    post_to_report(json="{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},{\"label\":\"Z\",\"value\":15}]}");
    pump(ms=400);
    // FeatureInfo=0 … Playback=8, Report=9 (see BrowserView inspector tabs).
    inspector_tab(index=9);
    pump(ms=400);
    mark("pass");
    detach_maps();
  }
}
