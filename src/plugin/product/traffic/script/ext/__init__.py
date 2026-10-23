# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.traffic_schemes — cost table over traffic.cost_quote."""

import json
import os


def path_metrics(path):
    """Read total_cost and vertex count from a path GeoJSON."""
    with open(path, encoding="utf-8") as handle:
        doc = json.load(handle)
    features = doc.get("features") or []
    if not features and doc.get("type") == "Feature":
        features = [doc]
    if not features:
        raise ValueError("path geojson has no features")
    props = features[0].get("properties") or {}
    if "cost" not in props:
        raise ValueError("path geojson has no cost")
    coords = features[0]["geometry"]["coordinates"]
    return {"total_cost": float(props["cost"]), "points": len(coords)}


def _write_table(out_dir, table):
    json_path = os.path.join(out_dir, "schemes.json")
    csv_path = os.path.join(out_dir, "schemes.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(table, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("id,weight_field,total_cost,points,path,selected\n")
        for row in table["rows"]:
            handle.write(
                "%s,%s,%.8g,%d,%s,%s\n"
                % (
                    row["id"],
                    row["weight_field"],
                    row["total_cost"],
                    row["points"],
                    row["path"],
                    "1" if row["selected"] else "0",
                )
            )
    return json_path, csv_path


def scheme_table(host, spec):
    """Quote each scheme, write the cost table, present the selected path."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    network = spec.get("network") or ""
    out_dir = spec.get("out_dir") or ""
    schemes = spec.get("schemes")
    selected = spec.get("selected")
    if not network or not os.path.isfile(network):
        return {"ok": False, "error": "路网不存在"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(schemes, list) or not schemes:
        return {"ok": False, "error": "schemes 必须是非空数组"}
    if not isinstance(selected, str) or not selected:
        return {"ok": False, "error": "selected 必须是方案 id"}
    if sum(1 for item in schemes if item.get("id") == selected) != 1:
        return {"ok": False, "error": "selected 必须是 schemes 中的唯一一档"}
    try:
        start_x = float(spec["start_x"])
        start_y = float(spec["start_y"])
        end_x = float(spec["end_x"])
        end_y = float(spec["end_y"])
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "起终点必须是数"}

    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for index, scheme in enumerate(schemes):
        if not isinstance(scheme, dict):
            return {"ok": False, "error": "方案必须是对象"}
        scheme_id = scheme.get("id") or ""
        weight = scheme.get("weight_field") or ""
        if not scheme_id:
            return {"ok": False, "error": "方案缺少 id"}
        path = os.path.join(out_dir, "path_%02d.geojson" % index)
        payload = {
            "network": network,
            "output": path,
            "start_x": start_x,
            "start_y": start_y,
            "end_x": end_x,
            "end_y": end_y,
            "frames": 1,
        }
        if weight:
            payload["weight_field"] = weight
        if not host.run_processing("traffic.cost_quote", json.dumps(payload)):
            return {"ok": False, "error": "方案 %s 的路径计算失败" % scheme_id}
        try:
            metrics = path_metrics(path)
        except (OSError, ValueError, KeyError, TypeError) as exc:
            return {"ok": False, "error": "无法读取路径费用：" + str(exc)}
        rows.append(
            {
                "id": scheme_id,
                "weight_field": weight,
                "total_cost": metrics["total_cost"],
                "points": metrics["points"],
                "path": path,
                "selected": scheme_id == selected,
            }
        )

    chosen = next(row for row in rows if row["selected"])
    table = {
        "network": network,
        "start_x": start_x,
        "start_y": start_y,
        "end_x": end_x,
        "end_y": end_y,
        "selected": selected,
        "cost_note": "native total_cost; map length when weight_field is empty",
        "rows": rows,
    }
    json_path, csv_path = _write_table(out_dir, table)
    present = {
        "network": network,
        "output": chosen["path"],
        "start_x": start_x,
        "start_y": start_y,
        "end_x": end_x,
        "end_y": end_y,
        "frames": 1,
    }
    if chosen["weight_field"]:
        present["weight_field"] = chosen["weight_field"]
    if not host.run_processing("traffic.cost_path", json.dumps(present)):
        return {
            "ok": False,
            "error": "对照表已写入 %s，选定方案未能呈现" % json_path,
            "table": json_path,
        }
    summary = (
        "方案 %s 费用 %s。对照表 %s" % (selected, chosen["total_cost"], json_path)
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
        "selected_cost": chosen["total_cost"],
    }
