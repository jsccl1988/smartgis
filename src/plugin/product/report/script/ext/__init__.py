# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""glang.ext.smartgis.report_deliver — conclusion JSON to an HTML report pack."""

import html
import json
import os


def _columns(rows, columns):
    if isinstance(columns, list) and columns:
        return [str(name) for name in columns]
    keys = []
    for key in rows[0].keys():
        keys.append(str(key))
    return keys


def _cell(row, column):
    if column not in row or row[column] is None:
        return ""
    return row[column]


def _write_html(path, title, columns, rows, selected):
    head = "".join("<th>%s</th>" % html.escape(name) for name in columns)
    body = []
    for index, row in enumerate(rows):
        mark = "1" if index == selected else "0"
        cells = "".join(
            "<td>%s</td>" % html.escape(str(_cell(row, name))) for name in columns
        )
        body.append('<tr data-selected="%s">%s</tr>' % (mark, cells))
    page = (
        "<!DOCTYPE html>\n"
        "<html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<title>%s</title>\n"
        "<style>\n"
        "body{font-family:sans-serif;margin:24px;color:#1a2330;"
        "background:#fafbfc}\n"
        "table{border-collapse:collapse;background:#fff}\n"
        "th,td{border:1px solid #d8dee6;padding:6px 10px;text-align:left}\n"
        "tr[data-selected=\"1\"]{background:#e8f1fb}\n"
        "</style></head><body>\n"
        "<h1>%s</h1>\n"
        "<p>选定行已标出。对照表保留全部档。</p>\n"
        "<table><thead><tr>%s</tr></thead><tbody>\n%s\n"
        "</tbody></table>\n</body></html>\n"
        % (html.escape(title), html.escape(title), head, "\n".join(body))
    )
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(page)


def _write_csv(path, columns, rows, selected):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(",".join(columns) + ",selected\n")
        for index, row in enumerate(rows):
            values = [str(_cell(row, name)) for name in columns]
            values.append("1" if index == selected else "0")
            handle.write(",".join(values) + "\n")


def materialize(host, spec):
    """Write index.html from conclusion rows and open that report once."""
    if not isinstance(spec, dict):
        return {"ok": False, "error": "报告参数必须是对象"}
    title = spec.get("title") or ""
    out_dir = spec.get("out_dir") or ""
    rows = spec.get("rows")
    if not title:
        return {"ok": False, "error": "缺少 title"}
    if not out_dir:
        return {"ok": False, "error": "缺少 out_dir"}
    if not isinstance(rows, list) or not rows:
        return {"ok": False, "error": "rows 必须是非空数组"}
    if not all(isinstance(row, dict) for row in rows):
        return {"ok": False, "error": "每一行必须是对象"}
    try:
        selected = spec["selected"]
        if isinstance(selected, bool) or not isinstance(selected, (int, float)):
            raise ValueError("selected type")
        index = int(selected)
        if index != selected:
            raise ValueError("selected fraction")
    except (KeyError, TypeError, ValueError):
        return {"ok": False, "error": "selected 必须是行号"}
    if index < 0 or index >= len(rows):
        return {"ok": False, "error": "selected 超出行范围"}

    columns = _columns(rows, spec.get("columns"))
    os.makedirs(out_dir, exist_ok=True)
    html_path = os.path.join(out_dir, "index.html")
    data_path = os.path.join(out_dir, "data.json")
    csv_path = os.path.join(out_dir, "table.csv")
    document = {
        "title": title,
        "selected": index,
        "columns": columns,
        "rows": rows,
    }
    with open(data_path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(document, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    _write_html(html_path, title, columns, rows, index)
    _write_csv(csv_path, columns, rows, index)
    if not host.run_processing(
        "report.open", json.dumps({"path": out_dir})
    ):
        return {
            "ok": False,
            "error": "报告已写入 %s，未能打开" % html_path,
            "html": html_path,
        }
    summary = "报告 %s 已落盘，选定第 %d 行。%s" % (title, index, html_path)
    return {
        "ok": True,
        "summary": summary,
        "html": html_path,
        "data": data_path,
        "csv": csv_path,
        "report_json": json.dumps(document, ensure_ascii=False),
    }
