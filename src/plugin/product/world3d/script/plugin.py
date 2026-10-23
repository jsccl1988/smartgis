# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""One-shot earth-system record for the native smartgis.world3d pack.

The native id is already smartgis.world3d, so this pack is
smartgis.world3d_earth and mounts glang.ext.smartgis.world3d_earth.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.world3d_earth as ext


PLUGIN_ID = "smartgis.world3d_earth"

_PAYLOAD_HINT = (
    "需要 JSON：out_dir。"
    "只调用 world3d.earth_system 一次，并记下它返回的 high_orbit / globe_dem / globe_terrain。"
    "不要同时打开平面海和地球。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "earth_brief"):
        raise RuntimeError("glang.ext.smartgis.world3d_earth missing")

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
        result = ext.earth_brief(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "太空天地海记录失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "world3d.earth_brief",
        "太空天地海记录",
        "tools",
        _run,
    )


def stop():
    pass
