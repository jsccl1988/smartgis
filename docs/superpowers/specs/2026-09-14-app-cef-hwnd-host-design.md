<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# CEF 产品壳：分区 HWND 宿主（方案 1）

**Date:** 2026-09-14  
**Status:** active  
**Goal:** 将真 CEF（Binary Distribution）落为与 Views / WinUI **产品设计完全对齐**的第三壳；第一版用 **分区 HWND**（Win32 顶层布局 + CEF chrome HWND + 独立地图子 HWND）交付可加载页面的 IDE 语义壳，地图仍走 `content` host ABI，不挖洞、不同 PE 硬塞 `--chrome=cef`。  
**Related:** [`docs/build/ui-shell-multiprocess.md`](../../build/ui-shell-multiprocess.md)、[`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md)、[`docs/build/ui-testing.md`](../../build/ui-testing.md)、[`src/app/views/README.md`](../../../src/app/views/README.md)、[`src/app/winui/map_host.*`](../../../src/app/winui/map_host.h)  
**Plan:** [`docs/superpowers/plans/2026-09-14-app-cef-hwnd-host.md`](../plans/2026-09-14-app-cef-hwnd-host.md)

## Locked decisions

| Topic | Choice |
| --- | --- |
| 目的 | CEF 回到**产品壳路径**（非纯 demo） |
| 三壳关系 | Views / WinUI / CEF **产品设计完全对齐**，并列可切换；Views 仍为方案 3 / 终局工具箱路径；默认构建**不编** CEF |
| 运行时 | 真 CEF Binary Distribution，pin 在 `third_party/`；一上来能加载本地/打包页面 + 原生地图 HWND |
| v1 验收 | 对齐 Views 单窗 IDE：菜单 / 停靠区语义齐（catalog / ambox / map tabs / inspector）；外壳 HTML/CSS；地图仍原生 HWND |
| 实现形态 | **方案 1 — 分区 HWND 宿主**（顶层 Win32 layout；chrome 区 CEF browser HWND；地图区独立子 HWND） |
| 否决 | 单 CEF 挖洞叠层；与 Views 同 PE `--chrome=cef` 作为第一刀；Qt；chrome 直接 include `gis_map` / `SmtRenderDevice` |
| 目录 / 产物 | `src/app/cef/` → `out/SmartGisCef.exe` |
| 核心类型 | `CefBrowserHost` / `CefMapSlot` / `ChromeBridge` / `web/` |
| GN | `smt_build_cef`（默认 `false`）、`smt_has_cef`；**不进** `all` / `src_all` |
| deps | `content` / `view_host` / `gpu` / `tool`；**不**依赖 `ui/views` 做壳；v1 **不**强依赖 `plugin:host`（Ambox 先灌 Workspace builtins；插件目录后挂） |
| 命令 / 自测 | command id 与 `--self-test` 对齐 [`ui-testing.md`](../../build/ui-testing.md) / Views |
| WinUI | 今日 IDE 布局已与 Views 对齐（Menu/Catalog/Ambox/Map tabs/Inspector/Status）；toolkit 仍为 Fluent，非终局 |

## 1. 背景 / 目标 / 非目标

### 1.1 背景

- 仓库桌面终局仍是 **Views + Skia**（[`ui-views-skia.md`](../../build/ui-views-skia.md)）。WinUI 是并列原型壳。
- [`ui-shell-multiprocess.md`](../../build/ui-shell-multiprocess.md) 原「方案 1」曾以 **WebView2** 为 web chrome，后已删除；文中亦曾把「CEF 只做 chrome」归为方案 3 变体 (c) 并标注不推荐。
- 产品需要一条 **真 Blink/Chromium 嵌入** 的 web chrome 轨：招人、HTML/CSS 迭代、与 Views 同语义的 IDE 布局，同时地图继续走本仓 `content` + 原生 HWND（或 OOP GPU），**不**把 `SmtMap` / GL 塞进 CEF GPU 进程。

### 1.2 目标

1. 独立 PE `SmartGisCef.exe`：opt-in 编译，默认不进日常 `group("all")`。
2. 分区 HWND：CEF 只画 chrome；地图槽是 sibling 子 HWND，由 `CefMapSlot` 对齐 HTML 里的 map slot 矩形。
3. 与 Views 对齐的 IDE 语义：菜单、Catalog、Ambox、Map|Data|3D tabs、FeatureInfo / AttributeTable inspector、StatusBar 语义（实现形态为 HTML/CSS + bridge，不是 `ui::views` 控件）。
4. 地图：`content::MapContents` / `MapView` / `ViewHost`；指针事件在地图 HWND 上直达 `ViewHost::dispatch_input` / `MapContents::Dispatch`，**不经 JS**。
5. ChromeBridge：版本化 JSON，command id 与 Views / `tool::Workspace` 一致。
6. `--self-test` 语义与退出码族对齐 Views（见 §8），并纳入未来 `exe_smoke`（缺二进制 SKIP）。

### 1.3 非目标

- 不把 CEF 升为终局壳（终局仍 Views）；不删 Views / WinUI。
- 不 vendor 整树 Chromium；只用 Binary Distribution pin。
- 不实现 CEF 挖洞 / off-screen 叠层地图；不做 WebGL 重画 `SmtMap`。
- 第一刀不做「同一 `SmartGisViews.exe` + `--chrome=cef`」。
- 不引入 Qt；chrome 禁止直接 `#include` `gis_map.h` / `rd_renderdevice.h` / leftover `SmtRenderDevice`。
- v1 不做完整 web 插件商店、不做 CEF 远程调试产品化、不把 WinUI 补齐到 IDE 布局作为前置。
- 本变更**只写设计文档**，不写实现代码、不 git commit。

## 2. 三壳对齐契约

三壳在产品层必须可互换理解；差异只在 **chrome 控件 toolkit**，不在地图语义。

| 契约项 | 共同约定 | CEF 侧落点 |
| --- | --- | --- |
| IDE 信息架构 | 单窗：Menu · Catalog · Map tabs · Ambox · Inspector · Status | `web/` 布局 + Win32 顶层框 |
| Map tabs | Map Edit / Data / 3D → `ViewKind` `kMapEdit` / `kMapData` / `kScene3d` | tab 切换驱动 `CefMapSlot` 显隐与 `OpenView` |
| 命令 id | `tool::Workspace` / Catalog 字符串（如 `selection.point`、`edit.append.point`、`view.pan`、`selection.clear`、`view3d.trackball`） | `ChromeBridge` → `ViewHost::execute` / `activate` |
| 地图 ABI | 仅 `content/public`（`MapContents`、`MapView`、`ViewHost`、`InputEvent`） | `CefMapSlot` + session |
| Present | 优先共享表面 / DIB present 进地图 HWND；不在 UI 跑 `SmtRenderDevice::Init` 作产品路径依赖（与 WinUI / Views 挂接策略一致，走 content） | 地图 HWND only present / host |
| 自测 | `--self-test`；退出码族见 `ui-testing.md` | `SmartGisCef.exe --self-test` |
| 构建门闩 | opt-in，不进 `all` / `src_all` | `smt_build_cef` + `smt_has_cef` |

**切换方式（产品意图，非第一刀实现）：** 用户装/编不同 PE（`SmartGisViews.exe` / `SmartGisWinui.exe` / `SmartGisCef.exe`），不是同 PE 运行时 `--chrome=`。日后若做启动器，只负责选 PE。

## 3. 架构（分区 HWND）

```text
SmartGisCef.exe (browser / UI process)
┌─────────────────────────────────────────────┐
│  Win32 top-level HWND（任务栏、加速键、拖放）   │
│  ┌──────────────┐  ┌──────────────────────┐ │
│  │ CEF browser  │  │ CefMapSlot child HWND │ │
│  │ HWND         │  │ (sibling，非 CEF 子树) │ │
│  │ HTML chrome  │  │ MapContents present   │ │
│  │ catalog/…    │  │ + ViewHost input      │ │
│  └──────────────┘  └──────────────────────┘ │
└─────────────────────────────────────────────┘
         │ ChromeBridge JSON          │ content host pipe / local
         ▼                            ▼
   command / catalog snapshot    GPU / renderer（见 §7）
```

**布局职责**

- **顶层 Win32**：创建外框、菜单加速键（可选原生菜单或全交 CEF）、DPI（Per-Monitor v2）、关闭/最小化、把客户区切成 chrome 矩形 + map 矩形。
- **CEF browser HWND**：填 chrome 矩形；加载 `web/`（开发可 `file://` 或自定义 scheme；发布拷到 exe 旁 `cef_web/`）。
- **CefMapSlot**：独立子 HWND，父为顶层（**不是** CEF 控件树子窗口）。根据页面 `data-map-slot`（或约定 id `#map-slot`）的客户端矩形，DPI 换算后 `SetWindowPos`；非活动 tab 的地图 HWND `SW_HIDE`（对齐 Views：非活动 HWND 不得盖住活动页）。

**明确不做**

- 不在单个 CEF view 上挖透明洞叠地图。
- 地图指针不进 JS；只有「从 web 面板拖图层到地图」等跨矩形手势才经宿主 `IDropTarget` 转发（v1 可降级为「仅按钮/命令打开图层」，拖放标为已知税，与旧 WebView2 方案同构）。

## 4. 目录与 GN

### 4.1 目录

```text
src/app/cef/
  BUILD.gn                 # win32_app / executable → SmartGisCef
  README.md
  main.cc                  # CefExecuteProcess → ContentMain / browser_main
  cef_app.*                # CefApp / 进程生命周期薄封装
  cef_browser_host.*       # 创建 browser、绑 parent HWND、生命周期
  cef_map_slot.*           # 地图子 HWND + MapContents / ViewHost 挂接
  chrome_bridge.*          # JS ↔ 宿主 JSON 协议
  layout_host.*            # 顶层 Win32 布局（chrome rect / map rect）
  self_test.cc             # --self-test（可与 main 同 TU）
  web/                     # HTML/CSS/JS 壳资源
    index.html
    css/
    js/
```

命名空间：公共符号最多两层（如 `app::cef` 内类型；内部放 `app::cef::detail`）。新函数 `snake_case`。

### 4.2 GN / 构建

| 项 | 约定 |
| --- | --- |
| 门闩 | `smt_build_cef` 默认 `false`；`build.bat cef`（或等价 alias）置真 |
| 探测 | `smt_has_cef`：Binary Distribution 头/库在 `third_party/` 约定路径存在（仿 `smt_has_flycube` / WinAppSDK） |
| 目标 | `//src/app/cef:cef` → `output_name = "SmartGisCef"`；根 `group("cef")` 仅当 `smt_build_cef && smt_has_cef` |
| 聚合 | **不**进 `//:all`、`//src:src_all` |
| deps | `//src/content:content`、`//src/content:view_host`、`//src/gpu:gpu_lib`、`//src/tool:dispatch`；**禁止**依赖 `//src/ui/views:views` 做壳 |
| 资源 | copy / action 把 `web/` → `out/cef_web/`（或 exe 旁相对路径，README 写死） |
| CEF 二进制 | 运行时 DLL/`.bin`/`icudtl` 等按 CEF 发行说明拷到 `out/`；版本 pin 写在 `build/smartgis.gni` 或 `third_party/cef/README` |

缺 CEF pin 时：目标不生成或 `build.bat cef` 打印人话错误（对齐 WinUI 缺 runtime），不静默。

## 5. 组件

### 5.1 `CefBrowserHost`

- 在顶层给定 parent HWND / 子矩形内创建 `CefBrowser`。
- 负责 `OnAfterCreated` / `DoClose` / 焦点；把浏览器 HWND 尺寸交给 `LayoutHost`。
- 加载起始 URL：发布态 `file://`/`cef_web/index.html` 或自定义 `smartgis://chrome/`（v1 允许 `file://`，自定义 scheme 可列为 plan 第二任务）。

### 5.2 `CefMapSlot`

- 创建/销毁地图子 HWND；`AttachMode` 对齐 Views `MapViewport`：优先 `content::MapView` / `MapContents::AttachSurface`。
- 每 tab 一个 slot 或单 slot 切换 `ViewKind`（**写死倾向：三 slot + 显隐**，与 Views 三 `MapViewport` 同构，避免切换时丢 surface generation）。
- `sync_layout(rect_px, dpi)`：由 `LayoutHost` 或 bridge 上报的 slot 矩形驱动。
- 输入：`WndProc` → `content::InputEvent` → 活动 `ViewHost::dispatch_input`（及/或 `MapContents::Dispatch`）。IME 组合留在 CEF；地图侧仅 `TextCommit`（若 v1 需要）。

### 5.3 `ChromeBridge`

- CEF `CefMessageRouter` 或 `ExecuteJavaScript` + `OnProcessMessageReceived` / `CefV8Handler` 二选一；**写死倾向：ProcessMessage JSON 双向**（少 V8 绑定面，易版本化）。
- 宿主侧解析后调用 `ViewHost` / `MapContents` / 本地 layout，**不**把 GIS 对象指针暴露给 JS。
- `api_version` 整型；不兼容时 JS 打日志并显示状态栏错误，禁止静默吞掉。

### 5.4 `web/`

- 静态壳：MenuBar 区、左 Catalog、中 TabStrip（Map|Data|3D）、右 Ambox、底 Inspector tabs、StatusBar。
- `#map-slot`（或每 tab 一个 slot）只负责**占位矩形**；不绘制地图像素。
- 命令按钮 / 树节点只发 bridge 消息（command id），不直连原生。

### 5.5 `LayoutHost`

- 测量客户区；向 CEF 通知 chrome 可用尺寸；根据页面回报的 slot 矩形摆 `CefMapSlot`。
- v1 允许「固定分区百分比 + 页面 postMessage 精调」；真拖拽 splitter 可在 HTML 做，矩形经 bridge 回推。

## 6. 数据流与 ChromeBridge 协议

### 6.1 运行时数据流

```text
[用户点击 Ambox「选择」]
  web/js → ChromeBridge {type:"ActivateTool", command_id:"selection.point", view_id}
    → ViewHost::execute / activate
    → 状态栏文案（宿主再 push Status）

[用户在地图 HWND 拖拽]
  WM_* → CefMapSlot → InputEvent → ViewHost::dispatch_input
    →（可选）EventBus SelectionChanged / ExtentChanged
    → ChromeBridge 推 JSON 快照给 inspector / status
  ※ 不进入 JS 热路径

[Catalog 图层开关]
  web → {type:"CatalogOp", op:...}
    → MapContents::CatalogCall(json) 或 ViewHost 等价路径
    → CatalogDelta / LegendSnapshot 回推 web 重绘树

[切换 Map|Data|3D]
  web → {type:"SelectMapTab", index:0|1|2}
    → LayoutHost 显隐对应 CefMapSlot
    → 必要时 OpenView / WaitFrameReady
```

### 6.2 消息形状（v1）

所有消息为 UTF-8 JSON 对象，公共字段：

| 字段 | 说明 |
| --- | --- |
| `api_version` | 整数；v1 = `1` |
| `type` | 见下表 |
| `request_id` | 需要应答的请求必填；事件可省略 |
| `view_id` | 地图视图；壳级消息可为 `0` |

**JS → 宿主（请求）**

| `type` | 载荷要点 | 宿主行为 |
| --- | --- | --- |
| `ActivateTool` | `command_id` | `ViewHost::execute` / `activate`（id 与 Views 相同） |
| `CatalogOp` | `op` JSON 字符串或对象 | `MapContents::CatalogCall` |
| `SelectMapTab` | `index` 0/1/2 | 切换 slot / ViewKind |
| `OpenFile` | 可选 path；空则宿主 `IFileDialog` | 对齐 Views Open |
| `LayoutSlot` | `slot_id`, `x,y,w,h` CSS px + `dpi` | 更新 `CefMapSlot` 几何 |
| `QueryState` | `what`: `status`/`selection`/`legend` | 立即回推快照 |
| `Exit` | — | 关进程 |

**宿主 → JS（事件 / 应答）**

| `type` | 载荷要点 |
| --- | --- |
| `Ack` / `Error` | `request_id`, `code`, `message` |
| `Status` | `text` |
| `SelectionChanged` | 不透明 feature id 列表（字节/hex），禁止原生指针 |
| `ExtentChanged` | `xmin,ymin,xmax,ymax` |
| `LegendSnapshot` / `CatalogDelta` | 与 content host 快照同语义 |
| `ViewCursor` | XY / 比例尺文案 |
| `Ready` | 壳与 map slot 首帧就绪（self-test 可用） |

**热路径禁令：** `mousemove` 地图坐标不得每帧进 JS。Extent/光标由宿主节流（例如 10–20 Hz）推 `ViewCursor` / `ExtentChanged`。

### 6.3 command id 对齐（权威来自 `tool::Workspace` + Views）

至少支持（与 Views `--self-test` 重叠者优先）：

- `selection.point` / `selection.clear`
- `edit.append.point`（及 linestring/polygon 可随后）
- `view.pan` / `view.zoom_in` / `view.zoom_out`
- `view3d.trackball`
- Catalog / Ambox：v1 用 JSON 列表灌进 web（至少 Workspace builtins）；**不**链 `ui::views::AmboxView`。与 Views `populate_ambox` 的 PluginHost 合并灌表留作后续任务，不阻塞第一版验收。

别名策略与 Views 一致：`select` / `identify` → `selection.point`，`pan` → `view.pan`（若 bridge 收到短名则宿主映射）。

## 7. CEF 进程模型与本仓 `--type=` 共存

### 7.1 问题

- CEF/Chromium 子进程使用 `--type=renderer` 等。
- 本仓 `content::ContentMain` / `MapContents::StartRenderProcess` 亦使用 `--type=gpu` / `--type=renderer`（见 `content/public/process_type.h`，Views / WinUI 同 PE 再拉起）。

### 7.2 写死策略（v1）

1. **`SmartGisCef.exe` 入口顺序（强制）**  
   `CefExecuteProcess` → 若返回值 `>= 0`，本进程是 CEF 子进程，直接退出；  
   否则进入 `content::ContentMain`（`browser_main` / `gpu_main` / `renderer_main` 回调与 Views 同构接线）。

2. **职责切割**  
   - CEF 多进程：只服务 HTML chrome（Blink / CEF GPU）。  
   - 地图 OOP：只服务 `MapContents` / `gpu`；**禁止**在 CEF renderer 进程加载 `Smt*` 地图 DLL。

3. **同 PE 再拉起（默认，对齐 WinUI README）**  
   `MapContents::StartRenderProcess` 继续 relaunch **本 PE** `SmartGisCef.exe --type=gpu`（及需要的 renderer）。因先走 `CefExecuteProcess`，无 CEF 通道参数的地图子进程应返回 `-1` 并落入 `ContentMain`。

4. **逃生舱（实现期若误判再启用，不作为第一刀默认）**  
   若发现 CEF 误吞本仓 `--type=renderer` 地图子进程：地图子进程改为旁路 `SmartGisRender.exe`（`smt_build_render`），CEF PE **不再**承载 ContentMain 的 `--type=`；bridge / `MapContents` 仍留在 browser 进程。此逃生舱需在实现 plan 中留一勾选，但 **产品文档默认叙述仍是同 PE ContentMain**。

5. **Job Object**  
   顶层 browser 退出时结束 CEF 子进程与地图 GPU/renderer 子进程（与 `ui-shell-multiprocess` 底物一致）。

```text
SmartGisCef.exe
  ├─ (CEF) --type=renderer / gpu-process / …   ← CefExecuteProcess 吃掉
  └─ (SMT) --type=gpu | --type=renderer      ← ContentMain 吃掉
```

## 8. 错误处理

| 场景 | 行为 |
| --- | --- |
| 缺 CEF Binary / `smt_has_cef=false` | 不生成 exe 或 `build.bat cef` 明确失败文案 |
| `CefInitialize` / 创建 browser 失败 | 日志 + 非零退出；self-test 专用码（见下） |
| `web/` 缺失或 index 404 | 状态栏 / 原生 MessageBox（self-test 抑模态）+ Error 事件；地图 slot 仍可尝试挂接以便诊断 |
| `MapContents::StartRenderProcess` 失败 | 地图 HWND 占位背景；`PresentStatus` 文案经 Status 推 web；不崩壳 |
| GPU / renderer 崩溃（`RenderDied`） | 丢弃旧 surface generation；可自动 `StartRenderProcess` 一次；失败则 Status 提示；chrome 保持 |
| Bridge `api_version` 不匹配 | `Error` + 拒绝执行写操作 |
| 未知 `command_id` | `Error`；不广播 leftover 插件（对齐 `ViewHost::execute_legacy` 纪律） |
| Catalog / Open 超时 | 默认 30s；弹/推错误，不卡死 UI 线程死等 |
| JS 异常 | 留在 CEF；宿主不因页面脚本失败而退出（self-test 仍以宿主断言为准） |

## 9. 测试与验收

### 9.1 分层（对齐 `ui-testing.md`）

| 层 | CEF 侧 |
| --- | --- |
| L0 | 不强制 `ui::views` 单测；可选 `chrome_bridge` 纯函数解析单测（无 HWND） |
| L1′ | **`SmartGisCef.exe --self-test`**（主门禁） |
| L2 | v1 **不做** CEF 像素金图（成本高）；壳外观靠 HTML 人工 / 后续再议 |
| L4 | `exe_smoke` 增加 `SmartGisCef.exe`：缺二进制 SKIP；`--require-all` 时若门闩开启则 FAIL |

### 9.2 `--self-test` 语义（对齐 Views）

顺序建议：

1. 创建顶层窗 + CEF 加载 `web/` + 至少一个 `CefMapSlot`。  
2. 顶层 HWND 有效。  
3. Map tab：`wait_ready`（content 挂接时）。  
4. Catalog / Ambox DOM 或 bridge `QueryState` 证明结构存在（不要求像素）。  
5. 切 Data / 3D：非活动地图 HWND 隐藏、活动可见（对齐 Views 36/37 族语义）。  
6. `view3d.trackball` 输入不崩。  
7. `edit.append.point` → undo 可用；`selection.point` / `selection.clear`；Status 文案。  
8. map HWND 与 slot 矩形对齐（容差与 Views 同类）。  

**退出码：** 与 Views **同号同义**处直接复用 [`ui-testing.md`](../../build/ui-testing.md)（0 通过；1 init；2 顶层 HWND；3/8/10 wait_ready；4–6 结构；7/9 native HWND；11–20 工具/状态栏；21–24 3D 输入；30–35 布局/几何；以及 Views 已用的非活动 HWND 可见性失败码）。CEF **独有**失败只用 **40+**，禁止把 40+ 挪去表示 Views 已有语义：

| 码 | 含义 |
| --- | --- |
| 40 | `CefInitialize` / browser 创建失败 |
| 41 | `web/` 未就绪 / 主 frame 加载失败 |
| 42 | Bridge `Ready` 超时 |
| 43 | Map slot 矩形无效（与 chrome 重叠或零面积） |

### 9.3 v1 验收清单（产品）

- [x] `smt_build_cef=true` 且 pin 存在时产出 `out/SmartGisCef.exe`；默认 `all` 不编。  
- [x] 启动可见 HTML 壳：Catalog / Ambox / Map tabs / Inspector 区域语义齐全。  
- [x] 地图原生 HWND 出帧（或占位 + 明确 Status）；指针不经 JS。  
- [x] 上述 command id 路径可走通。  
- [x] `--self-test` 退出 0（在 CI/本地有 CEF 的配置下）。  
- [x] chrome 源文件无 `gis_map` / `SmtRenderDevice` include。

## 10. 与现有文档的关系

| 文档 | 关系 / 后续动作 |
| --- | --- |
| [`ui-shell-multiprocess.md`](../../build/ui-shell-multiprocess.md) | **意图更新（后续 as-built 修订，可与实现同 PR）：** §1「方案 1」从已删除的 WebView2 改为 **CEF chrome + 分区 HWND + native map**（宿主形态仍 sibling HWND，协议仍 `content` host）。§3.1 变体 (c)「CEF 不推荐」与 §4.4「明确不选 CEF」**仅针对「用 CEF 冒充方案 3 / 终局 Views」**；**不**否定本 spec 的第三壳产品路径。底物 §0 不变。 |
| [`ui-views-skia.md`](../../build/ui-views-skia.md) | Views 仍为终局；CEF / WinUI 为并列可切换壳。可在「Rejected / alternate」中注明 CEF 壳见本 spec，避免与「WebView2 sibling」旧叙述混淆。 |
| [`ui-testing.md`](../../build/ui-testing.md) | 实现期把 `SmartGisCef.exe` 写入 L1′/L4 表。 |
| [`src/app/views`](../../../src/app/views/README.md) | 语义与布局参考实现；CEF **不**依赖其代码。 |
| [`src/app/winui/map_host`](../../../src/app/winui/map_host.h) | HWND 挂图 / `sync_layout` 参考；CEF 用 Win32 而非 XAML island。 |
| `2026-09-14-app-legacy-split-design` | `src/app/` 终局树增加 `cef/` 并列；不恢复 MFC。 |

本 spec **不**在本变更中大改 `ui-shell-multiprocess.md` 正文；把「方案 1 = CEF 分区 HWND」的修订列为实现 plan / 文档回写任务，以免设计与 as-built 长文纠缠。

## 11. 否决项（再次冻结）

1. 单 CEF 视图挖洞 / 离屏叠层地图。  
2. 第一刀：`SmartGisViews.exe --chrome=cef` 同 PE 切壳。  
3. Qt 或任何第二套 widget 库。  
4. chrome 直接链接 GIS/Render 设备头。  
5. 用 CEF/WebGL 重实现 `SmtMap`。  
6. 默认构建编进 CEF / 把 CEF 塞进 `src_all`。  
7. 把 CEF 写成「方案 3 终局」替代 Views。

## 12. 开放问题（写死倾向）

| 问题 | 倾向 |
| --- | --- |
| 自定义 scheme vs `file://` | v1 `file://` + `out/cef_web/`；scheme 留 plan 可选项 |
| Bridge 用 MessageRouter 还是裸 ProcessMessage | ProcessMessage JSON |
| 三 map slot vs 单 slot 切换 | 三 slot + 显隐（对齐 Views） |
| 同 PE ContentMain vs 旁路 `SmartGisRender.exe` | 默认同 PE + `CefExecuteProcess` 优先；误判再旁路 |
| WinUI IDE 布局 | 不阻塞 CEF；WinUI 另里程碑追同契约 |
| CEF 版本 / 位宽 | 与本仓 MSVC 工具链一致的官方 Binary Dist；具体版号在 pin 目录 README 写死（实现首任务） |
| 菜单用原生还是 HTML | v1 HTML；加速键可由 Win32 转发为 bridge 命令 |

## 13. 实现 plan 边界（供下一步 writing-plans）

建议单份 plan 可覆盖的切片：

1. GN 门闩 + third_party CEF pin 探测 + 空壳 exe 启动 CEF 加载 `web/index.html`。  
2. `LayoutHost` + `CefMapSlot` HWND + `MapContents` 挂接出帧。  
3. `ChromeBridge` v1 消息表 + Ambox/Catalog 命令。  
4. Map|Data|3D 与 ViewHost self-test 路径。  
5. `exe_smoke` / `ui-testing.md` 挂接 +（可选）`ui-shell-multiprocess` §1 表述回写。

超出范围：WinUI 追平、插件 web 扩展生态、像素金图、同 PE `--chrome=`。

---

**最后更新：** 2026-09-14
