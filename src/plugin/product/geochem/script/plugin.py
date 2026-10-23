# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Threshold exceedance table for the native smartgis.geochem pack.

The native id is already smartgis.geochem, so this pack is
smartgis.geochem_grades and mounts glang.ext.smartgis.geochem_grades.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.geochem_grades as ext


PLUGIN_ID = "smartgis.geochem_grades"

_PAYLOAD_HINT = (
    "需要 JSON：input, element, out_dir, k_sigmas, selected。"
    "k_sigmas 是正的阈值倍数数组，selected 必须是其中一档。"
    "超标数是样点值大于等于该档 threshold 的个数，不跑 IDW。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "exceedance"):
        raise RuntimeError("glang.ext.smartgis.geochem_grades missing")

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
        result = ext.exceedance(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "超标对照失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        report = result.get("report_json") or ""
        if report:
            host.post_to_report(report)
        return True

    host.contribute_command(
        PLUGIN_ID,
        "geochem.exceedance",
        "品位阈值超标对照",
        "tools",
        _run,
    )


def stop():
    pass
