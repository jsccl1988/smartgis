# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tile coverage and scale bar for the native smartgis.map2d pack.

The native id is already smartgis.map2d, so this pack is
smartgis.map2d_coverage and mounts glang.ext.smartgis.map2d_coverage.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.map2d_coverage as ext


PLUGIN_ID = "smartgis.map2d_coverage"

_PAYLOAD_HINT = (
    "需要 JSON：template, west, south, east, north, zooms, selected, out_dir。"
    "template 含 {z}/{x}/{y}。zooms 是缩放级别数组，selected 必须是其中一级。"
    "只打开选定级别对应的金字塔一次。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "coverage"):
        raise RuntimeError("glang.ext.smartgis.map2d_coverage missing")

    def _run(payload):
        text = payload or ""
        if not str(text).strip():
            gp.ui.show_message_box("error", _PAYLOAD_HINT)
            return False
        try:
            spec = json.loads(text)
        except json.JSONDecodeError as exc:
            gp.ui.show_message_box("error", "对照参数不是 JSON：" + str(exc))
            return False
        result = ext.coverage(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "瓦片覆盖对照失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "map2d.coverage",
        "瓦片覆盖与比例尺",
        "tools",
        _run,
    )


def stop():
    pass
