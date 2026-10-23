# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.world3d_earth — record one world3d.earth_system report.

The native command takes no payload (see scene/earth/register.cc). This pack
calls it once and writes the JSON it reports. Planar ocean and the globe are
not requested together.
"""

import json
import os


_OCEAN_LOOKS = ("ocean", "full", "coast", "legacy")

_FIELDS = (
    "op",
    "mode",
    "space",
    "sky",
    "sat_cloud",
    "land",
    "sea",
    "planar_ocean",
    "planar_cloud",
    "fog",
    "china_stack",
    "contour",
    "hex",
    "ortho",
    "wind",
)


def _asks_planar_ocean(spec):
    if spec.get("planar_ocean") is True or spec.get("ocean") is True:
        return True
    look = spec.get("look") or ""
    return isinstance(look, str) and look in _OCEAN_LOOKS


def _globe_on(report):
    land = report.get("land")
    sea = report.get("sea")
    mode = report.get("mode")
    return (
        land == "globe_dem"
        or sea == "globe_terrain"
        or mode in ("earth_system", "globe")
        or report.get("globe") is True
    )


def _read_report(host):
    reader = getattr(host, "operation_result", None)
    if not callable(reader):
        return None, "宿主没有 operation_result，无法记下命令回报"
    text = reader()
    if text is None:
        return None, "world3d.earth_system 没有回报"
    if isinstance(text, bytes):
        text = text.decode("utf-8", "replace")
    text = str(text).strip()
    if not text:
        return None, "world3d.earth_system 没有回报"
    try:
        report = json.loads(text)
    except ValueError as exc:
        return None, "命令回报不是 JSON：" + str(exc)
    if not isinstance(report, dict):
        return None, "命令回报必须是对象"
    return report, ""


def _csv_cell(value):
    if isinstance(value, bool):
        return "1" if value else "0"
    if value is None:
        return ""
    return str(value)


def _write_record(out_dir, report):
    json_path = os.path.join(out_dir, "earth_brief.json")
    csv_path = os.path.join(out_dir, "earth_brief.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("field,value\n")
        for field in _FIELDS:
            if field in report:
                handle.write("%s,%s\n" % (field, _csv_cell(report[field])))
    return json_path, csv_path


def earth_brief(host, spec):
    """Call world3d.earth_system once and store the fields it reports."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    out_dir = spec.get("out_dir") or ""
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if _asks_planar_ocean(spec):
        return {
            "ok": False,
            "error": "平面海与地球不能同时打开，未调用 world3d.earth_system",
        }

    # The native handler ignores its payload. Do not send a second look.
    if not host.run_processing("world3d.earth_system", "{}"):
        return {"ok": False, "error": "world3d.earth_system 未能执行"}
    report, error = _read_report(host)
    if report is None:
        return {"ok": False, "error": error}
    if report.get("ok") is False or report.get("error"):
        return {
            "ok": False,
            "error": report.get("error") or "world3d.earth_system 失败",
        }
    if report.get("planar_ocean") is True and _globe_on(report):
        return {
            "ok": False,
            "error": "回报同时打开了平面海和地球，已拒绝记录",
        }
    for key in ("space", "land", "sea"):
        if key not in report:
            return {"ok": False, "error": "回报缺少 " + key}

    os.makedirs(out_dir, exist_ok=True)
    json_path, csv_path = _write_record(out_dir, report)
    summary = (
        "太空天地海：space=%s land=%s sea=%s。记录 %s"
        % (report.get("space"), report.get("land"), report.get("sea"), json_path)
    )
    posted = dict(report)
    posted["table"] = json_path
    posted["csv"] = csv_path
    posted["summary"] = summary
    return {
        "ok": True,
        "summary": summary,
        "table": json_path,
        "csv": csv_path,
        "report_json": json.dumps(posted, ensure_ascii=False),
        "space": report.get("space"),
        "land": report.get("land"),
        "sea": report.get("sea"),
        "planar_ocean": report.get("planar_ocean"),
    }
