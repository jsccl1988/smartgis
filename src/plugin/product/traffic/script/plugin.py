# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Scheme cost table for the native smartgis.traffic pack.

The native id is already smartgis.traffic, so this pack is
smartgis.traffic_schemes and mounts glang.ext.smartgis.traffic_schemes.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.traffic_schemes as ext


PLUGIN_ID = "smartgis.traffic_schemes"

_PAYLOAD_HINT = (
    "需要 JSON：network, out_dir, start_x, start_y, end_x, end_y, schemes, selected。"
    "schemes 是 {id, weight_field} 数组，weight_field 空表示路段长度。"
    "selected 必须是其中一档 id。费用是 native total_cost，空权重时为地图长度。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "scheme_table"):
        raise RuntimeError("glang.ext.smartgis.traffic_schemes missing")

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
        result = ext.scheme_table(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "方案对照失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "traffic.scheme_table",
        "路径方案费用对照",
        "tools",
        _run,
    )


def stop():
    pass
