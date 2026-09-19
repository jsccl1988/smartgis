<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GUI / Views 测试方案

本仓桌面壳终局是 **Views + Skia**（[`ui-views-skia.md`](ui-views-skia.md)）。GUI 测试投资跟 `SmartGisViews.exe` / `ui::views` 走；leftover MFC（`SmartGis.exe`）只保进程冒烟。

业界对照：单测 → 进程内交互（Chromium [Kombucha](https://chromium.googlesource.com/chromium/src/+/main/chrome/test/interaction/README.md)）→ 壳像素（本仓 L2 为本地 PNG + WIC，非 Skia Gold）→ 黑盒 UIA → 产品冒烟。本仓已落在 **L0 + L1′ + L2 + L4**。

## 分层

| 层 | 名称 | 本仓状态 | 入口 |
| --- | --- | --- | --- |
| **L0** | 工具箱单测（合成事件） | **已有** | `out\views_unittests.exe` |
| **L1** | 进程内交互序列 | 规划（仿 Kombucha 轻量 API） | 未来 `views_interactive_tests` |
| **L1′** | 产品壳语义路径 | **已有** | `SmartGisViews.exe --self-test`；`SmartGisWinui.exe --self-test`；`SmartGisCef.exe --self-test`（有 CEF pin 时） |
| **L2** | 壳像素回归 | **已有**；地图帧不进默认基线 | `out\views_pixel_tests.exe` |
| **L3** | 黑盒 UIA / FlaUI | **低优先**（自绘 Views 缺 Provider） | 暂缓 |
| **L4** | 产品 exe 冒烟 | **已有** | `build.bat e2e` → `exe_smoke` |

GN / 跑法总入口：[`testing/README.md`](../../testing/README.md)。

## 原则

1. **Views 为主、MFC 为辅** — 新用例只加在 `ui::views` / `app/views`；`SmartGis.exe` 仅 `exe_smoke`。
2. **白盒优先于 UIA** — 合成 `MouseEvent` / `KeyEvent`、直接查 View 树与命令状态；不靠屏幕坐标点图。
3. **地图断言走语义** — `MapViewport::wait_ready`、`ViewHost`、`EditSession`、图层/状态栏文案；不断言地图像素。
4. **像素（若做）只测壳** — MenuBar / Tab / StatusBar / 对话框；固定 DIP、关动画；不测 GPU 地图帧。
5. **禁止** — Qt / Squish-for-Qt；不以 WinAppDriver 为主轨；不对 leftover MFC 写大规模 FlaUI。

## 已有能力

### L0 — `views_unittests`

- 路径：`src/ui/views/testing/views_unittests.cc`（GN：`//src/ui/views:views_unittests`）。
- 控制台自测：无 MFC、多数用例不 `CreateWindow`；合成鼠标/键盘驱动 kernel / primitives / GIS 面板。
- 覆盖示例：Theme / Skia canvas API、焦点与 Tab 遍历、BoxLayout、Button / Textfield / Checkbox / Radio / Combobox、TabStrip、Table / AttributeTable、LayerTree、Splitter、ScrollView、**AmboxView**（`CommandCatalog` 分组 + 滚动内容 + `layout_check`）、MenuBar、DPI、`layout_check`。
- 模态挡板：`set_message_box_suppressed_for_test` / `set_file_picker_modals_suppressed_for_test`。
- 布局不变量：`ui/views/kernel/layout_check.h`（`collect_layout_violations`；无 golden 图）。

```bat
build.bat te
out\views_unittests.exe
```

### L1′ — `SmartGisViews.exe --self-test`

- 实现：`src/app/views/main.cc`（`BrowserMain`）。
- 真 HWND：泵消息 → 检查壳 → Map / Data / 3D 切换 → `wait_ready`（`kContentMapView` 时）→ 断言 `HostView::Latest` 出帧（marks：`map-frame-ok` / `scene-frame-ok`）→ 3D trackball 输入 → 编辑点 / 选择 / 清选 → OGR China PLP 进层（`china-plp-ok`）→ `view.pan`（`pan-ok`）→ 轨道相机矩阵 → `layout_check` → 地图 HWND 与 View bounds 对齐。
- 由 `exe_smoke` 拉起；窗口标题 `SmartGIS Views`。
- C# 壳：`SmartGisCs.exe --self-test`（`build.bat cs`）。

| 退出码（节选） | 含义 |
| --- | --- |
| 0 | 通过 |
| 1 | `BrowserView::init` 失败 |
| 2 | 顶层 HWND 无效 |
| 3 / 8 / 10 | Map / Data / Scene `wait_ready` 超时 |
| 4–6 | 内容树 / Catalog 结构异常 |
| 7 / 9 | Data / Scene native HWND 无效 |
| 11–20 | ViewHost / 工具 / 状态栏语义失败 |
| 21–25 | 3D trackball / 输入分发 / 相机未动 |
| 26–29 | OGR 进层失败 / 轨道相机矩阵 / FlyCube present |
| 30–35 | 布局不变量或地图 HWND 几何失败 |
| 36–38 | 图层 / Catalog 空或 HWND 显隐 |
| 39 | China PLP 包络不在中国经纬度范围 |
| 40–42 | `view.pan` 激活或输入分发失败 |

### L1′ — Atmosphere 3D showcase（`SmartGisViews.exe --atmosphere-showcase=`）

独立于完整 `--self-test`：切到 3D 页，按模式配置大气，连续 `present_gpu` 三帧后退出。
默认 **Null RHI**（确定性 exit 0）。`SMT_ATMOSPHERE_SHOWCASE_GPU=1` 时在 3D HWND
上拉 FlyCube/DX12（启动期勿设 `SMT_PREFER_FLYCUBE_3D=1`，多 viewport attach 易挂死；
本机 post-detach FlyCube `present_gpu` 也曾挂起）。旁路产物：`out/atmosphere-showcase-mark.txt`、
`atmosphere-showcase-<mode>.bmp`、`atmosphere-showcase-cmdline.txt`。

| 模式 | 含义 |
| --- | --- |
| `land` | 大气未挂载；仅 land/DEM present |
| `ocean` | procedural 场 + ocean on / cloud off |
| `full` | `enable_atmosphere_demo()`（海+云） |
| `coast` | 东海附近 extent + full demo |

| 退出码 | 含义 |
| --- | --- |
| 0 | 通过 |
| 1 / 2 | init / 顶层 HWND（与自测同） |
| 50 | 3D viewport HWND 缺失 |
| 51 | device `create` / `initialize` 失败 |
| 52 | `present_gpu` 失败 |
| 53 | 大气开关或 FieldStore 状态不符 |

```bat
set SMT_RUN_FLYCUBE_GPU=1
out\SmartGisViews.exe --atmosphere-showcase=land
out\SmartGisViews.exe --atmosphere-showcase=ocean
out\SmartGisViews.exe --atmosphere-showcase=full
out\SmartGisViews.exe --atmosphere-showcase=coast
```

### L1′ — `SmartGisWinui.exe --self-test`

- 实现：`src/app/winui/application.cc`（`OnLaunched`）。
- IDE 区域：MenuBar / Catalog / Ambox / Map|Data|3D / Inspector / StatusBar（与 Views 同语义）。
- 2D（`kMapEdit`）与 3D（`kScene3d`）均要求 `WaitFrameReady` + 非占位 DIB（marks：`map-frame-ok` / `scene-frame-ok`）；Data 页同样 `wait`。
- 窗口标题 `SmartGIS WinUI`；cwd 建议 `out/`。

### L1′ — `SmartGisCs.exe --self-test`

- 实现：`src/app/cs/SmartGisCs/MainWindow.cs`（`RunSelfTestAsync`）。
- 语义与 WinUI C++ **同号同义**；需 `SmartGisRender.exe` 旁路 GPU。
- 窗口标题 `SmartGIS WinUI (C#)`；cwd 建议 `out/`。

### L1′ — `SmartGisCef.exe --self-test`

- 实现：`src/app/cef/self_test.cc`（经 `main.cc` `BrowserMain`）。
- 语义与 Views **同号同义**（上表）；CEF 独有失败只用 **40+**（禁止挪用 Views 语义）：

| 码 | 含义 |
| --- | --- |
| 40 | `CefInitialize` / browser 创建失败 |
| 41 | `web/` 未就绪 / 主 frame 加载失败 |
| 42 | Bridge `Ready` 超时 |
| 43 | Map slot 矩形无效（与 chrome 重叠判定失败或零面积） |

- 需 `smt_build_cef=true` 且 `third_party/cef` Binary Dist pin；缺 pin 时不编 exe（`build.bat cef` 明确失败）。
- 窗口标题 `SmartGIS CEF`。
- 出帧 marks：`map-frame-ok` / `scene-frame-ok`（与 Views / WinUI 同名）。

### L2 — `views_pixel_tests`

- 路径：`src/ui/views/testing/views_pixel_tests.cc`（GN：`//src/ui/views:views_pixel_tests`）；离屏 GDI 捕获 + PNG 基线（WIC 读写）。
- 脚手架：`src/ui/views/testing/pixel_harness.{h,cc}`、`pixel_png_wic.cc`。
- 基线目录：`src/ui/views/testing/testdata/*.png`（壳控件 only：Label+Button、TabStrip、StatusBar、Ambox 默认条；不含 MapViewport 像素）。
- 环境：96 DIP（`device_scale_factor = 1`）、Segoe UI 12px 与 `Theme::measure_text_utf8` 对齐；比较时默认每通道 ±2、坏点比例 ≤ 0.5%。
- 更新基线（仓库根目录 cwd，与 `build.bat te` 一致）：

```bat
out\views_pixel_tests.exe --update-goldens
```

- 已接入 `//:test_all`；`build.bat te` 会编译并运行。

### L4 — `exe_smoke`

- 路径：`testing/e2e/exe_smoke.cc`。
- 对各 PE 执行 `--self-test`；缺二进制默认 SKIP，`--require-all` 则 FAIL。

| Binary | `--self-test` 证明 |
| --- | --- |
| `SmartGisRender.exe` | OOP GPU + `FrameReady` + 共享表面 |
| `SmartGisViews.exe` | Views 窗 + 地图挂接 / 语义路径 |
| `SmartGisWinui.exe` | WinUI IDE 壳 + Map/Data/3D 出帧（marks：`map-frame-ok` / `scene-frame-ok`） |
| `SmartGisCef.exe` | CEF chrome + 分区 HWND 地图；缺二进制 SKIP |
| `SmartGis.exe` | MFC 主框出现（模态卡住时 harness 关窗） |

```bat
build.bat e2e
```

## 关键路径（应覆盖 / 已覆盖）

| 路径 | 状态 |
| --- | --- |
| 启壳 → Catalog / Ambox / StatusBar 布局 | L1′ + `layout_check` |
| Map \| Data \| 3D 切换 + HWND + `wait_ready` | L1′ |
| 3D trackball 输入不崩 | L1′ |
| `edit.append.point` → undo 可用 → 状态栏 | L1′ |
| `selection.point` / `selection.clear` | L1′ |
| 工具箱控件行为（无真窗） | L0 |
| 壳控件外观（离屏 PNG） | L2 |
| Open 真实工程 → 图层树 → FeatureInfo | **待扩**（假数据夹具） |
| Create Layer / Basemap 对话框 | **待扩**（继续用 modal suppress） |

## 分期

| 阶段 | 内容 | 产出 |
| --- | --- | --- |
| **P0** | `views_unittests` 已入 `//:test_all`；`build.bat te` 会编译并跑 L0；`build.bat e2e` 跑 L1′+L4 | `te` / `e2e` 绿 |
| **P1** | 接入 gtest；按模块拆 `views_unittests`；`--self-test=suite` 可选过滤 | 可过滤套件 |
| **P2** | 轻量交互序列 fixture（进程内 Click → Wait → CheckView） | `views_interactive_tests` |
| **P3** | 扩展 L2 场景 + 假数据夹具覆盖 Open 路径 | 外观 + 数据回归 |

## 不做 / 慎做

- 不引入 Qt 测试栈。
- 不以 WinAppDriver / 大规模 FlaUI 作为 Views 主方案。
- 不对 leftover MFC 写深度 UI 自动化。
- 不把地图渲染帧纳入默认 pixel 基线（驱动 / GPU 差异会炸 CI）。
- 不在无 `AutomationId` Provider 时依赖 UIA Name / 坐标点击。

## 相关

| 文档 / 代码 | 角色 |
| --- | --- |
| [`ui-views-skia.md`](ui-views-skia.md) | UI 终局 |
| [`testing/README.md`](../../testing/README.md) | GN 测试入口 |
| [`src/ui/views/README.md`](../../src/ui/views/README.md) | 工具箱 + `views_unittests` |
| [`src/app/views/README.md`](../../src/app/views/README.md) | 产品壳 + `--self-test` |
| `src/ui/views/kernel/layout_check.h` | 布局不变量 |
| `src/ui/views/testing/testdata/` | L2 PNG 基线与说明 |

---

**最后更新：** 2026-09-19
