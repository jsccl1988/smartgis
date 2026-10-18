# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Shared digitize clicks. Callers own marks and any feature_count check.

from steps.support import fail_kwargs


def append_point(sg, tool_fail_rc=None, expect_fail_rc=None, after_tool=None):
    sg.tool(id="edit.append.point", **fail_kwargs(tool_fail_rc))
    if after_tool is not None:
        after_tool()
    sg.expect_tool(id="draw.point", **fail_kwargs(expect_fail_rc))
    sg.click("map_client", 12, 18)
    sg.expect_geom(kind="point", min_points=1)


def append_line(sg, fail_rc=None):
    sg.tool(id="edit.append.linestring", **fail_kwargs(fail_rc))
    sg.expect_tool(id="draw.linestring", **fail_kwargs(fail_rc))
    sg.click("map_client", 20, 20)
    sg.click("map_client", 80, 60)
    sg.rclick("map_client", 80, 60)
    sg.expect_geom(kind="linestring", min_points=2)


def append_polygon(sg, fail_rc=None):
    sg.tool(id="edit.append.polygon", **fail_kwargs(fail_rc))
    sg.expect_tool(id="draw.polygon", **fail_kwargs(fail_rc))
    sg.click("map_client", 30, 30)
    sg.click("map_client", 90, 30)
    sg.click("map_client", 60, 80)
    sg.rclick("map_client", 60, 80)
    sg.expect_geom(kind="polygon", min_points=3)
