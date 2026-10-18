# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Report dock integration (WebView2 / fake bridge).
# IR: horizon HWND → plugin report sample → inspector tab.

import glang.plugin as gp

def main():
    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.require_plugins()
    rep = gp.resolve_data(kind="plugin", leaf="report/sample")
    if not rep:
        gp.mark("report-sample-missing")
        raise RuntimeError("report-sample-missing")
    gp.open_report(path=rep)
    gp.mark("report-ok")
    gp.post_to_report(json="{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},{\"label\":\"Z\",\"value\":15}]}")
    gp.pump(ms=400)
    gp.inspector_tab(index=9)
    gp.pump(ms=400)
    gp.mark("pass")
