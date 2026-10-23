# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.mine_reserves — prism volume table over stratum pairs."""

import json
import os


def _write_table(out_dir, table):
    json_path = os.path.join(out_dir, "reserves.json")
    csv_path = os.path.join(out_dir, "reserves.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(table, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "id,top_stratum_id,bottom_stratum_id,volume_map,selected\n"
        )
        for row in table["rows"]:
            handle.write(
                "%s,%s,%s,%.8g,%s\n"
                % (
                    row["id"],
                    row["top_stratum_id"],
                    row["bottom_stratum_id"],
                    row["volume_map"],
                    "1" if row["selected"] else "0",
                )
            )
    return json_path, csv_path


def reserve_table(host, spec):
    """Volume each stratum pair, write the table, present the selected top."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    source = spec.get("input") or ""
    out_dir = spec.get("out_dir") or ""
    pairs = spec.get("pairs")
    selected = spec.get("selected")
    if not source or not os.path.isfile(source):
        return {"ok": False, "error": "钻孔表不存在"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(pairs, list) or not pairs:
        return {"ok": False, "error": "pairs 必须是非空数组"}
    if not isinstance(selected, str) or not selected:
        return {"ok": False, "error": "selected 必须是地层对 id"}
    if sum(1 for pair in pairs if pair.get("id") == selected) != 1:
        return {"ok": False, "error": "selected 必须是 pairs 中的唯一一档"}

    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for index, pair in enumerate(pairs):
        if not isinstance(pair, dict):
            return {"ok": False, "error": "地层对必须是对象"}
        pair_id = pair.get("id") or ""
        top_id = pair.get("top_stratum_id") or ""
        bottom_id = pair.get("bottom_stratum_id") or ""
        if not pair_id or not top_id or not bottom_id:
            return {"ok": False, "error": "地层对缺少 id 或顶底板"}
        vol_path = os.path.join(out_dir, "volume_%02d.json" % index)
        payload = json.dumps(
            {
                "input": source,
                "top_stratum_id": top_id,
                "bottom_stratum_id": bottom_id,
                "output": vol_path,
            }
        )
        if not host.run_processing("mine.prism_volume", payload):
            return {"ok": False, "error": "地层对 %s 的体积计算失败" % pair_id}
        try:
            with open(vol_path, encoding="utf-8") as handle:
                vol = json.load(handle)
            volume = float(vol["volume"])
        except (OSError, ValueError, KeyError, TypeError) as exc:
            return {"ok": False, "error": "无法读取体积：" + str(exc)}
        rows.append(
            {
                "id": pair_id,
                "top_stratum_id": top_id,
                "bottom_stratum_id": bottom_id,
                "volume_map": volume,
                "volume_file": vol_path,
                "selected": pair_id == selected,
            }
        )

    chosen = next(row for row in rows if row["selected"])
    table = {
        "input": source,
        "selected": selected,
        "volume_unit": "map^3",
        "rows": rows,
    }
    json_path, csv_path = _write_table(out_dir, table)
    present = json.dumps(
        {"input": source, "stratum_id": chosen["top_stratum_id"]}
    )
    if not host.run_processing("mine.interpolate_stratum", present):
        return {
            "ok": False,
            "error": "对照表已写入 %s，选定顶板未能呈现" % json_path,
            "table": json_path,
        }
    summary = (
        "地层对 %s 体积 %s（地图单位³）。对照表 %s"
        % (selected, chosen["volume_map"], json_path)
    )
    report = dict(table)
    report["table"] = json_path
    report["csv"] = csv_path
    report["summary"] = summary
    return {
        "ok": True,
        "summary": summary,
        "table": json_path,
        "csv": csv_path,
        "report_json": json.dumps(report, ensure_ascii=False),
        "selected_volume": chosen["volume_map"],
    }
