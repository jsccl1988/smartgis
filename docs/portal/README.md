<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Docs Portal

本地 **docs 展示插件**：自动扫描 `docs/` 下的 Markdown 与 HTML（含 `superpowers/diagrams/*.html`），侧栏目录 + 右侧预览。零第三方依赖（仅 Python 标准库）。

## 启动

仓库根或本目录：

```bat
docs\portal\open.bat
py -3 docs\portal\serve.py
py -3 docs\portal\serve.py --port 8765 --no-browser
```

浏览器打开 `http://127.0.0.1:8765/`（端口占用时自动换口）。

深链：`http://127.0.0.1:8765/?path=superpowers/diagrams/views-window-process.html`

## 能力

| | |
| --- | --- |
| 自动目录 | `GET /api/catalog` 每次请求重扫 `docs/` |
| MD 预览 | 拉取原文，前端轻量渲染（标题 / 列表 / 表格 / 代码块 / 链接） |
| HTML 页面 | iframe 整页嵌入（设计原理图可交互） |
| 过滤 | 全部 / 仅 MD / 仅 HTML + 路径搜索 |
| 静态资源 | `docs/` 下相对路径按原样提供，HTML 图内资源可用 |

不要直接双击打开 `index.html`（需要本服务提供 `/api/catalog`）。

## 文件

| 文件 | 作用 |
| --- | --- |
| `serve.py` | 扫描 + HTTP |
| `index.html` | UI（无 CDN） |
| `open.bat` | Windows 一键启动 |
