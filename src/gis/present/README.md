<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/present`

`present` 产出 `StyleDocument`、符号和瓦片，是地图帧的输入。`gis::vista::Layout` 消费它们。它不是 Frame Graph，也不是领域。

`gis::World` 与 `gis::DomainSession` 不解析 Style JSON。样式在 `style/`，瓦片在 `tile/`，都不搬进 `vista`。
