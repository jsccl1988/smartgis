# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.geochem_grades — exceedance table over threshold tallies."""

import json
import os


def _write_table(out_dir, table):
    json_path = os.path.join(out_dir, "exceedance.json")
    csv_path = os.path.join(out_dir, "exceedance.csv")
    with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(table, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    with open(csv_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(
            "k_sigma,threshold,background,mean,count,exceed_count,selected\n"
        )
        for row in table["rows"]:
            handle.write(
                "%.8g,%.8g,%.8g,%.8g,%d,%d,%s\n"
                % (
                    row["k_sigma"],
                    row["threshold"],
                    row["background"],
                    row["mean"],
                    row["count"],
                    row["exceed_count"],
                    "1" if row["selected"] else "0",
                )
            )
    return json_path, csv_path


def exceedance(host, spec):
    """Tally one threshold per k_sigma, write the table, present selected."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "对照参数必须是对象"}
    source = spec.get("input") or ""
    element = spec.get("element") or ""
    out_dir = spec.get("out_dir") or ""
    k_sigmas = spec.get("k_sigmas")
    if not source or not os.path.isfile(source):
        return {"ok": False, "error": "样点表不存在"}
    if not element:
        return {"ok": False, "error": "缺少 element"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(k_sigmas, list) or not k_sigmas:
        return {"ok": False, "error": "k_sigmas 必须是非空数组"}
    try:
        selected = float(spec["selected"])
        k_sigmas = [float(k) for k in k_sigmas]
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "k_sigmas 和 selected 必须是数"}
    if any(k <= 0 for k in k_sigmas) or selected <= 0:
        return {"ok": False, "error": "k_sigma 必须为正数"}
    if not any(abs(k - selected) <= 1e-6 for k in k_sigmas):
        return {"ok": False, "error": "selected 必须是 k_sigmas 中的一档"}

    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for index, k_sigma in enumerate(k_sigmas):
        tally_path = os.path.join(out_dir, "tally_%02d.json" % index)
        payload = json.dumps(
            {
                "input": source,
                "element": element,
                "k_sigma": k_sigma,
                "output": tally_path,
            }
        )
        if not host.run_processing("geochem.threshold_tally", payload):
            return {"ok": False, "error": "阈值倍数 %s 的统计失败" % k_sigma}
        try:
            with open(tally_path, encoding="utf-8") as handle:
                tally = json.load(handle)
        except (OSError, ValueError) as exc:
            return {"ok": False, "error": "无法读取超标统计：" + str(exc)}
        rows.append(
            {
                "k_sigma": k_sigma,
                "threshold": tally.get("threshold"),
                "background": tally.get("background"),
                "mean": tally.get("mean"),
                "count": tally.get("count"),
                "exceed_count": tally.get("exceed_count"),
                "tally": tally_path,
                "selected": abs(k_sigma - selected) <= 1e-6,
            }
        )

    chosen = next(row for row in rows if row["selected"])
    table = {
        "input": source,
        "element": element,
        "selected": selected,
        "rows": rows,
    }
    json_path, csv_path = _write_table(out_dir, table)
    present = json.dumps(
        {
            "input": source,
            "element": element,
            "k_sigma": selected,
            "bins": 10,
        }
    )
    if not host.run_processing("geochem.stats", present):
        return {
            "ok": False,
            "error": "对照表已写入 %s，选定阈值分级未能呈现" % json_path,
            "table": json_path,
        }
    summary = (
        "元素 %s 在 k=%s（阈值 %s）有 %s 个样点达到或超过阈值。对照表 %s"
        % (element, selected, chosen["threshold"], chosen["exceed_count"], json_path)
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
        "selected_exceed_count": chosen["exceed_count"],
    }
