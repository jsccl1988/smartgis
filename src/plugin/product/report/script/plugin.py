# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Openable HTML deliverable for the native smartgis.report pack.

The native id is already smartgis.report, so this pack is
smartgis.report_deliver and mounts glang.ext.smartgis.report_deliver.
"""

import json

import glang.plugin as gp
import glang.ext.smartgis.report_deliver as ext


PLUGIN_ID = "smartgis.report_deliver"

_PAYLOAD_HINT = (
    "需要 JSON：title, out_dir, rows, selected。"
    "rows 是对象数组，selected 是要标出的行号（从 0 起）。"
    "会写成 index.html，并只打开这一份报告。"
)


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "materialize"):
        raise RuntimeError("glang.ext.smartgis.report_deliver missing")

    def _run(payload):
        text = payload or ""
        if not str(text).strip():
            gp.ui.show_message_box("error", _PAYLOAD_HINT)
            return False
        try:
            spec = json.loads(text)
        except json.JSONDecodeError as exc:
            gp.ui.show_message_box("error", "报告参数不是 JSON：" + str(exc))
            return False
        result = ext.materialize(host, spec)
        if not result.get("ok"):
            gp.ui.show_message_box(
                "error", result.get("error") or "报告落盘失败"
            )
            return False
        gp.ui.show_message_box("info", result.get("summary") or "")
        return True

    host.contribute_command(
        PLUGIN_ID,
        "report.materialize",
        "结论报告落盘",
        "tools",
        _run,
    )


def stop():
    pass
