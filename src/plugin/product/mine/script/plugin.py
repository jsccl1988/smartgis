# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Stratum-pair volume table for the native smartgis.mine pack.

The native id is already smartgis.mine, so this pack is
smartgis.mine_reserves and mounts glang.ext.smartgis.mine_reserves.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.mine_reserves as ext


PLUGIN_ID = "smartgis.mine_reserves"

_PAYLOAD_HINT = (
    "需要 JSON：input, out_dir, pairs, selected。"
    "pairs 是 {id, top_stratum_id, bottom_stratum_id} 数组，"
    "selected 必须是其中一档 id。"
    "体积是地图单位的三次方，不是自动换算的立方米。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "reserve_table"):
        raise RuntimeError("glang.ext.smartgis.mine_reserves missing")

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
        result = ext.reserve_table(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "体积对照失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "mine.reserve_table",
        "地层对体积对照",
        "tools",
        _run,
    )


def stop():
    pass
