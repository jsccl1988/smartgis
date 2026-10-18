# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Pan, zoom, browse burst, and wheel-to-cursor.
# navigate() is the harness sequence (pan, burst, wheel). Zoom stays
# callable so a suite can opt in without changing that mark order.

from steps.support import fail_kwargs


def pan_drag(sg, fail_rc=None, expect_fail_rc=None):
    sg.tool(id="view.pan", **fail_kwargs(fail_rc))
    sg.expect_tool(id="view.pan", **fail_kwargs(expect_fail_rc))
    sg.drag("map_client", 40, 40, 70, 55)
    sg.mark("pan-ok")


def zoom_in(sg, fail_rc=None, expect_fail_rc=None):
    # view.zoom_in is a rubber-band rect on the map client.
    sg.tool(id="view.zoom_in", **fail_kwargs(fail_rc))
    sg.expect_tool(id="view.zoom_in", **fail_kwargs(expect_fail_rc))
    sg.drag("map_client", 30, 30, 90, 80)
    sg.mark("zoom-in-ok")


def zoom_out(sg, fail_rc=None, expect_fail_rc=None):
    # view.zoom_out is a point stroke.
    sg.tool(id="view.zoom_out", **fail_kwargs(fail_rc))
    sg.expect_tool(id="view.zoom_out", **fail_kwargs(expect_fail_rc))
    sg.click("map_client", 48, 48)
    sg.mark("zoom-out-ok")


def browse_burst(sg, count):
    sg.browse_stress(count=count)
    sg.mark("browse-ok")


def wheel_cursor(sg, x=40, y=40):
    sg.expect_wheel_cursor(x=x, y=y)
    sg.mark("wheel-cursor-ok")


def navigate(sg):
    pan_drag(sg, fail_rc=40, expect_fail_rc=41)
    sg.stop_map_timers()
    browse_burst(sg, 24)
    sg.resume_map_timers()
    wheel_cursor(sg)


def main():
    import glang.plugin as gp

    navigate(gp)
