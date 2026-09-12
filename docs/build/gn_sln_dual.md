<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 工程入口：只有 GN

mogu 的双轨是 GN + Bazel。本仓**不搬 Bazel**，也**不把 sln 当第二主轨**。

| 入口 | 状态 |
| --- | --- |
| `build.bat` → `gn gen` + `ninja` | **唯一工程入口** |
| 已删除的 `vs2008/` / `branches/` | 不再存在；`build.bat sln` 仍 **拒绝** |

默认 `//:all` = `//src:src_all`（不依赖 MFC / D3DX9 的已接线 DLL）。主程序与 D3D 设备尚未进 GN 组；缺 MFC / DirectX SDK 时那些目标会硬停，不要假装已绿。

---

**最后更新：** 2026-09-13
