# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# DebugAgent console gate (:help / :layers / :extent / :refresh + pan bench).
# IR lanes: horizon (HWND / debug) -> document (china) -> view (tool / bench).

import glang.plugin as gp

def main():
    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.wait_viewport(face="map", ms=20000, frame=0)
    gp.mark("map-ready")
    gp.stop_map_timers()
    gp.wire_debug_agent()
    china = gp.resolve_data(kind="data", leaf="china_city.gpkg")
    if not china:
        gp.mark("china-seed-miss")
        raise RuntimeError("china seed missing")
    gp.open_map(path=china, fail_rc=26)
    gp.fit_extent()
    gp.pump(ms=100)
    gp.debug_exec(line=":help", contains=":layers", fail_rc=51)
    gp.debug_exec(line=":layers", reject="no host.layer_names", fail_rc=52)
    gp.debug_exec(line=":extent", reject="empty", fail_rc=53)
    gp.debug_exec(line=":refresh", equals="refreshed", fail_rc=54)
    gp.mark("console-ok")
    gp.tool(id="view.pan", fail_rc=40)
    gp.console_pan_bench()
    gp.mark("console-bench-ok")
    gp.mark("pass")
    gp.mark("detached")
