// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Open local HTML report sample in the Report dock (WebView2).

script "plugin.report" {
  seq @inproc {
    require_plugins();
    resolve_data(kind="plugin", leaf="report/sample", as="rep");
    require_var(name="rep", fail_mark="report-sample-missing");
    open_report(path="$rep");
    mark("report-ok");
    // FeatureInfo=0 … Playback=8, Report=9 (see BrowserView inspector tabs).
    inspector_tab(index=9);
    pump(ms=800);
    post_to_report(json="{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},{\"label\":\"Z\",\"value\":15}]}");
    pump(ms=1200);
    mark("pass");
    detach_maps();
  }
}
