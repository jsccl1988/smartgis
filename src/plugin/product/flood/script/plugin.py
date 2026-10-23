# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Water-level inundation curve for the native smartgis.flood pack.

The native id is already smartgis.flood, so this pack is
smartgis.flood_levels and mounts glang.ext.smartgis.flood_levels.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.flood_levels as ext


PLUGIN_ID = "smartgis.flood_levels"

_PAYLOAD_HINT = (
    "需要 JSON：dem, out_dir, seed_x, seed_y, levels, selected。"
    "levels 是水位数组，selected 必须是其中一档。"
    "面积单位是 DEM 的地图单位，不是自动换算的平方米。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "level_curve"):
        raise RuntimeError("glang.ext.smartgis.flood_levels missing")

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
        result = ext.level_curve(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "水位对照失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "flood.level_curve",
        "水位淹没面积对照",
        "tools",
        _run,
    )


def stop():
    pass
