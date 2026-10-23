<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# plugin/runtime/samples

新样品树（与冻结的 `python/samples/` 脱钩）。演示 **glang 核心**（`import glang.plugin`）与 **第三方 extension**（`import glang.ext.<plugin_id>`）。

| 包 | `plugin.json` id | 说明 |
| --- | --- | --- |
| `hello_ext/` | `smartgis.sample_hello_ext` | `ext/` 纯 Python + `plugin.py` 同时用核心与 ext |

挂载由 `plugin/runtime/extension` 在 Registry python start 时调用 `content::py::mount_extension`。规范见 living **§glang.ext 扩展面**。
