<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# CEF 分区 HWND 产品壳 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付独立 PE `out/SmartGisCef.exe`：真 CEF Binary Distribution 画 HTML chrome，分区 HWND 挂原生地图（`content` / `ViewHost`），产品语义与 Views / WinUI 对齐；默认不编进 `all` / `src_all`。

**Architecture:** 顶层 Win32 客户区切成 chrome 矩形 + map 矩形。CEF browser HWND 只填 chrome；三枚 `CefMapSlot`（Map|Data|3D）为顶层 sibling 子 HWND，经 `MapContents` / `MapView` / `ViewHost` 出帧与输入。`ChromeBridge` 用版本化 ProcessMessage JSON（`api_version=1`）传命令与快照；地图指针不经 JS。入口强制 `CefExecuteProcess` → `content::ContentMain`（同 PE 再拉起 `--type=gpu|renderer`）。

**Tech Stack:** C++23、GN/Ninja（`out/` only）、CEF Binary Distribution（`third_party/cef` pin）、Win32、`content::{MapContents,MapView,ViewHost,ContentMain}`、`tool::Workspace`、静态 `web/` HTML/CSS/JS。

**Spec:** [`docs/superpowers/specs/2026-09-14-app-cef-hwnd-host-design.md`](../specs/2026-09-14-app-cef-hwnd-host-design.md)

## Global Constraints

- 工作目录 `C:/Dev/src/gis/smartgis`，只在 **`master`** 改；不要开分支。
- **不要 `git commit`**，除非用户明确要求。
- 产品壳路径；Views / WinUI / CEF **三壳并列产品对齐**（IDE：menu / catalog / ambox / map tabs / inspector）；WinUI IDE 补齐**不阻塞**本 plan。
- 真 CEF Binary Distribution pin；**方案 1 分区 HWND**（禁止挖洞 / OSR 叠层地图）。
- 目录 / 产物：`src/app/cef/` → `SmartGisCef.exe`；`smt_build_cef` 默认 `false`；**不进** `all` / `src_all`。
- ChromeBridge JSON；地图 `CefMapSlot` 原生 HWND → `content` ViewHost / MapContents。
- `--self-test` 对齐 [`docs/build/ui-testing.md`](../../build/ui-testing.md)；退出码 0–35 与 Views **同号同义**；CEF 独有只用 **40+**。
- **无 Qt**；chrome **禁止**直接 `#include` `gis_map.h` / `rd_renderdevice.h` / leftover `SmtRenderDevice`；**禁止**依赖 `//src/ui/views:views` 做壳。
- v1 **不**强依赖 `plugin:host`（Ambox 先灌 Workspace builtins）。
- 公共命名空间 ≤2（`app::cef`；内部 `app::cef::detail`）；新函数 `snake_case`。
- Copyright：新/改动文件 `Copyright (c) 2026 The Mogu Authors.`；源码注释英文；本 plan / as-built 文档中文。
- 代码查找用 CBM `user-codebase-memory-mcp` project=`smartgis`（`root_path`=`C:/Dev/src/gis/smartgis`）。

## File map

| Path | Responsibility |
| --- | --- |
| `third_party/cef/README.md` | Binary Dist 版号 / 解压布局 / 位宽 / 运行时 DLL 列表 |
| `src/app/cef/cef.gni` | `smt_build_cef`（默认 false）+ `smt_has_cef` 探测 |
| `src/app/cef/BUILD.gn` | `executable("cef")` → `SmartGisCef`；copy `web/` + CEF 运行时 |
| `src/app/cef/README.md` | 构建 / 运行 / `--self-test` / 与 Views 对齐说明 |
| `src/app/cef/main.cc` | `CefExecuteProcess` → `ContentMain`；`--self-test` 调度 |
| `src/app/cef/cef_app.h` / `.cc` | `CefApp` / 生命周期薄封装 |
| `src/app/cef/cef_browser_host.h` / `.cc` | 创建 browser、绑 parent HWND、加载 `cef_web/` |
| `src/app/cef/layout_host.h` / `.cc` | 顶层 Win32 布局：chrome rect / map rect / DPI |
| `src/app/cef/cef_map_slot.h` / `.cc` | 三 slot 子 HWND + MapContents / ViewHost 挂接与输入 |
| `src/app/cef/chrome_bridge.h` / `.cc` | ProcessMessage JSON 解析 / 派发 / 回推 |
| `src/app/cef/chrome_bridge_test.cc` | L0：无 HWND 的 JSON 解析单测（可选但推荐） |
| `src/app/cef/self_test.cc` | `--self-test` 语义路径（可与 `main.cc` 同 TU） |
| `src/app/cef/web/index.html` + `css/` + `js/` | IDE 壳：Menu / Catalog / Tabs / Ambox / Inspector / Status；`#map-slot` 占位 |
| `BUILD.gn`（根） | `group("cef")`；**不**写入 `group("all")` / `e2e` 强制 deps |
| `build.bat` | `cef` alias：`BUILD_CEF=true` + `NINJA_TARGET=cef` |
| `build/BUILDCONFIG.gn` | 仅当需要时声明全局 `smt_build_cef`；优先放 `cef.gni`（仿 WinUI） |
| `testing/e2e/exe_smoke.cc` | 增加 `SmartGisCef.exe`（缺二进制 SKIP） |
| `docs/build/ui-testing.md` | L1′ / L4 表写入 `SmartGisCef.exe` + 40+ 码 |
| `docs/build/ui-shell-multiprocess.md` | §1 方案 1 表述回写为 CEF 分区 HWND（Task 5） |
| `docs/README.md` | Active 索引挂 plan（本轮最小挂链） |

**明确不做（实现时跳过）：**

- Qt；CEF 挖洞 / OSR 叠层；`SmartGisViews.exe --chrome=cef`。
- 默认编进 `all` / `src_all`；依赖 `ui/views` 做壳。
- chrome 直接碰 `gis_map` / `SmtRenderDevice`。
- WinUI IDE 布局追平（**已在 2026-09-15 与 Views 同语义落地**，见 `src/app/winui/README.md`）；插件 web 商店；CEF 像素金图；自定义 `smartgis://` scheme（v1 用 `file://`）。
- 把旁路 `SmartGisRender.exe` 当默认（仅 Task 5 逃生舱勾选）。

---

### Task 1: CEF pin + GN 门闩 + 空壳加载 `web/index.html`

**Files:**
- Create: `third_party/cef/README.md`
- Create: `src/app/cef/cef.gni`
- Create: `src/app/cef/BUILD.gn`
- Create: `src/app/cef/README.md`
- Create: `src/app/cef/main.cc`
- Create: `src/app/cef/cef_app.h`, `src/app/cef/cef_app.cc`
- Create: `src/app/cef/cef_browser_host.h`, `src/app/cef/cef_browser_host.cc`
- Create: `src/app/cef/layout_host.h`, `src/app/cef/layout_host.cc`（本任务最小：顶层窗 + chrome 矩形填满客户区）
- Create: `src/app/cef/web/index.html`, `src/app/cef/web/css/shell.css`, `src/app/cef/web/js/bridge.js`（最小壳：标题 + 「CEF chrome ready」）
- Modify: `BUILD.gn`（根 `group("cef")`）
- Modify: `build.bat`（`cef` alias + `smt_build_cef`）
- Modify: `src/app/BUILD.gn`（注释列出 `cef/`）

**Interfaces:**
- Produces: `smt_build_cef` / `smt_has_cef`；`//src/app/cef:cef` → `SmartGisCef.exe`；`app::cef::CefApp`；`app::cef::CefBrowserHost::create(parent, rect, url)`；`app::cef::LayoutHost` 顶层 HWND
- Consumes: CEF Binary Dist 头/库；不依赖 `content` 地图挂接（本任务可先不链 `MapContents`，或链上但不 OpenView）

- [x] **Step 1: 写 `third_party/cef/README.md`（版号写死）**

约定布局（实现首任务必须选定并写入具体 CEF 发行版号，与本仓 MSVC / x64 一致；示例骨架，**把 `CEF_VERSION` 换成实际 pin**）：

```markdown
<!-- Copyright (c) 2026 The Mogu Authors. All rights reserved. -->
# CEF Binary Distribution pin

- Dist: `cef_binary_<CEF_VERSION>_windows64`
- Unpack to: `third_party/cef/binary/` (gitignored contents OK; README tracked)
- Required for `smt_has_cef=true`:
  - `binary/include/cef_version.h`
  - `binary/Release/libcef.lib` (or Debug twin per config)
  - Runtime: `libcef.dll`, `chrome_elf.dll`, `icudtl.dat`, `*.bin`, `locales/`, …
- Not vendored Chromium source tree.
```

若本机尚无 pin：本任务仍写齐 GN/源码，但 `smt_has_cef=false` 时 `group("cef")` 为空；`build.bat cef` 打印人话错误（对齐 WinUI 缺 runtime）。

- [x] **Step 2: 写 `src/app/cef/cef.gni`**

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

declare_args() {
  smt_build_cef = false
}

_cef_ver_h = exec_script("//build/file_exists.py",
                         [ "third_party/cef/binary/include/cef_version.h" ],
                         "trim string")
smt_has_cef = _cef_ver_h == "1"
```

- [x] **Step 3: 写最小 `BUILD.gn` + 根 `group("cef")`**

`src/app/cef/BUILD.gn` 要点：

```gn
import("cef.gni")
import("//build/config/win/application.gni")

if (smt_build_cef && smt_has_cef) {
  config("cef_cc") {
    include_dirs = [
      "//src",
      "//third_party/cef/binary",
      "//third_party/cef/binary/include",
    ]
    lib_dirs = [ "//third_party/cef/binary/Release" ]
    libs = [ "libcef.lib", "libcef_dll_wrapper.lib" ]
    defines = [ "WRAPPING_CEF_SHARED", "UNICODE", "_UNICODE" ]
  }

  copy("cef_web_assets") {
    sources = [
      "web/index.html",
      "web/css/shell.css",
      "web/js/bridge.js",
    ]
    outputs = [ "$root_out_dir/cef_web/{{source_file_part}}" ]
    # Prefer preserving css/ js/ subdirs via explicit outputs if needed.
  }

  win32_app("cef") {
    output_name = "SmartGisCef"
    sources = [
      "main.cc",
      "cef_app.cc",
      "cef_browser_host.cc",
      "layout_host.cc",
    ]
    configs += [ ":cef_cc" ]
    deps = [
      ":cef_web_assets",
      # Task 2+ adds content / view_host / gpu_lib / tool:dispatch
    ]
    libs = [ "shell32.lib", "user32.lib", "gdi32.lib" ]
  }

  group("SmartGisCef") {
    deps = [ ":cef" ]
  }
}
```

根 `BUILD.gn`：

```gn
import("//src/app/cef/cef.gni")

group("cef") {
  deps = []
  if (smt_build_cef && smt_has_cef) {
    deps += [ "//src/app/cef:cef" ]
  }
}
```

**禁止**把 `:cef` 加入 `group("all")` / `//src:src_all`。`group("e2e")` 可在 Task 5 再按 `smt_build_cef` 可选加入（与 winui 同构）。

- [x] **Step 4: `build.bat` 增加 `cef` alias**

在现有 `winui` 分支旁：

```bat
) else if /I "%~1"=="cef" (
  set "NINJA_TARGET=cef"
  set "BUILD_CEF=true"
```

`gn gen` args 追加 `smt_build_cef=!BUILD_CEF!`（默认 `BUILD_CEF=false`）。若 `BUILD_CEF=true` 且随后 ninja 无目标 / `smt_has_cef=false`，在 bat 里探测 `third_party\cef\binary\include\cef_version.h` 缺失则 echo：

`CEF Binary Distribution missing. See third_party/cef/README.md`

- [x] **Step 5: 实现入口顺序 + 最小顶层窗 + CEF 加载页面**

`main.cc` 强制顺序（与 spec §7.2）：

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <windows.h>
#include <shellapi.h>

#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"

#include "app/cef/cef_app.h"
#include "app/cef/cef_browser_host.h"
#include "app/cef/layout_host.h"
#include "content/app/content_main.h"
#include "content/app/renderer_main.h"
#include "gpu/gpu.h"

namespace {

int BrowserMain(const content::ContentMainParams& params) {
  app::cef::LayoutHost layout;
  if (!layout.create(params.instance)) {
    return 1;
  }
  CefMainArgs main_args(static_cast<HINSTANCE>(params.instance));
  CefRefPtr<app::cef::CefApp> app(new app::cef::CefApp());
  CefSettings settings;
  settings.no_sandbox = true;
  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return 40;  // CEF-only code even outside --self-test
  }
  const std::wstring url = layout.cef_web_index_url();  // file:///.../cef_web/index.html
  app::cef::CefBrowserHost host;
  if (!host.create(layout.hwnd(), layout.chrome_rect(), url)) {
    CefShutdown();
    return 40;
  }
  layout.show();
  // Message loop: CefRunMessageLoop or integrate with GetMessage + CefDoMessageLoopWork.
  CefRunMessageLoop();
  host.destroy();
  CefShutdown();
  return 0;
}

int GpuMain(const content::ContentMainParams& params) {
  return gpu::GpuMain(params.argc, params.argv);
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  CefMainArgs main_args(instance);
  const int cef_rc = CefExecuteProcess(main_args, nullptr, nullptr);
  if (cef_rc >= 0) {
    return cef_rc;
  }

  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.browser_main = &BrowserMain;
  params.gpu_main = &GpuMain;
  params.renderer_main = &content::RendererMain;
  const int rc = content::ContentMain(params);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
```

`web/index.html` 最小内容：可见标题「SmartGIS CEF」、区域占位注释 `#map-slot`（本任务可先不驱动原生 slot）。

- [x] **Step 6: 验证门闩与空壳**

Run:

```bat
build.bat all
```

Expected: 成功且 **不**产出 `out\SmartGisCef.exe`。

Run（有 pin）:

```bat
build.bat cef
dir out\SmartGisCef.exe
out\SmartGisCef.exe
```

Expected: 可见 HTML 壳窗口；关闭后进程退出。

Run（无 pin）:

```bat
build.bat cef
```

Expected: 人话错误或空 `group("cef")`，**不**静默假装成功产出 exe。

- [x] **Step 7: 更新 `src/app/cef/README.md` + `src/app/BUILD.gn` 注释**

写明：`build.bat cef`、`smt_build_cef` / `smt_has_cef`、`out/cef_web/`、入口 `CefExecuteProcess`→`ContentMain`、与 Views 并列、不进 `all`。

---

### Task 2: `LayoutHost` 分区 + 三 `CefMapSlot` + `MapContents` 出帧

**Files:**
- Modify: `src/app/cef/layout_host.h`, `layout_host.cc`
- Create: `src/app/cef/cef_map_slot.h`, `cef_map_slot.cc`
- Modify: `src/app/cef/main.cc` / `BrowserMain`（创建 session、三 slot、初始布局）
- Modify: `src/app/cef/BUILD.gn` deps：`//src/content:content`、`//src/content:view_host`、`//src/gpu:gpu_lib`、`//src/tool:dispatch`
- Modify: `src/app/cef/web/*`（固定分区百分比布局；`#map-slot-edit|data|scene` 或单 `#map-slot` + tab 切换）

**Interfaces:**
- Consumes: Task 1 `LayoutHost` / `CefBrowserHost`；`content::MapContents::Create` / `StartRenderProcess` / `OpenView` / `AttachSurface`；`content::MapView`；`content::ViewHost`；`content::ViewKind`；`content::InputEvent`；`content::PresentMode`
- Produces:
  - `app::cef::CefMapSlot::{create,destroy,sync_layout,set_visible,wait_ready,view_host,native_hwnd,view_id}`
  - `LayoutHost::{chrome_rect,map_rect_for_tab,set_active_tab,hwnd}`
  - 三 slot：`kMapEdit` / `kMapData` / `kScene3d`，非活动 `SW_HIDE`

参考（只读对齐，勿依赖其代码）：

- `src/app/winui/map_host.*` — 子 HWND + `sync_layout` + present
- `src/app/views/main.cc` — 三 viewport 显隐与 `wait_ready`
- `src/content/public/map_contents.h`、`map_view.h`、`view_host.h`、`map_types.h`

- [x] **Step 1: 扩展 `LayoutHost` 分区几何**

客户区默认比例（v1 可写死，后续由 bridge `LayoutSlot` 精调）：

- 左 Catalog 约 20%、中 Map 约 55%、右 Ambox 约 25%；底 Inspector / Status 固定高度；顶 Menu 固定高度。
- `chrome_rect()` = 客户区减去 **活动 map 槽**矩形（CEF 仍画完整 IDE chrome；map 槽由 sibling HWND 盖住 `#map-slot` 占位）。
- 或：CEF 客户区为「除 map 槽外的 L 形 / 多矩形」——v1 **写死倾向**：顶层客户区 = 全窗；CEF browser HWND = 全客户区；**三个 map HWND 叠在 `#map-slot` 屏幕矩形上**（与旧 WebView2 sibling 同构）。`SetWindowPos` 按 bridge/页面矩形；无 bridge 前用固定客户区中部矩形作占位。

```cpp
namespace app {
namespace cef {

struct RectPx {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

class LayoutHost {
 public:
  bool create(void* instance);
  void show();
  HWND hwnd() const;
  RectPx chrome_rect() const;       // CEF browser bounds in client px
  RectPx map_slot_rect() const;     // active map slot in client px
  float dpi_scale() const;
  std::wstring cef_web_index_url() const;
  void set_map_slot_rect(const RectPx& r);
  void set_active_tab(int index);   // 0/1/2
  int active_tab() const;
};

}  // namespace cef
}  // namespace app
```

- [x] **Step 2: 实现 `CefMapSlot`（三实例）**

```cpp
namespace app {
namespace cef {

class CefMapSlot {
 public:
  bool create(HWND parent,
              content::MapContents* session,
              content::ViewKind kind);
  void destroy();
  void sync_layout(const RectPx& rect_px, float dpi);
  void set_visible(bool visible);
  bool wait_ready(uint32_t timeout_ms);
  HWND native_hwnd() const;
  uint32_t view_id() const;
  content::ViewHost* view_host();
  content::ViewKind kind() const;

  static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);

 private:
  // WndProc → content::InputEvent → view_host_->dispatch_input
  // Prefer MapView Create(parent_hwnd) + set_present_mode(kSoftwareDib or kSharedTexture)
  // Do NOT include gis_map / SmtRenderDevice headers.
};

}  // namespace cef
}  // namespace app
```

创建流程（对齐 WinUI / Views content 挂接）：

1. `session->StartRenderProcess()`（失败：占位背景 + Status 文案，不崩壳）。
2. `view_id_ = session->OpenView(kind)`。
3. `content::MapView`（或 `AttachSurface`）绑 `child_hwnd_`；`PresentMode` 优先共享表面 / DIB，与 WinUI `MapHost` 一致走 content，不在 UI 跑 `SmtRenderDevice::Init`。
4. 每 slot 自有 `content::ViewHost`（或共享一个 host + 按 tab 切换 `view_id`——**写死倾向：每 slot 一个 `ViewHost`**，与 Views 三 viewport 同构）。

输入：地图 HWND 上 `WM_LBUTTON*` / `WM_MOUSEMOVE` / `WM_MOUSEWHEEL` / `WM_KEY*` → `InputEvent` → `dispatch_input`；**不**转发到 JS。IME 组合留在 CEF。

- [x] **Step 3: `BrowserMain` 接线三 slot**

```cpp
content::MapContents* session = content::MapContents::Create();
session->StartRenderProcess();

app::cef::CefMapSlot slots[3];
slots[0].create(layout.hwnd(), session, content::ViewKind::kMapEdit);
slots[1].create(layout.hwnd(), session, content::ViewKind::kMapData);
slots[2].create(layout.hwnd(), session, content::ViewKind::kScene3d);

auto sync_all = [&]() {
  const int active = layout.active_tab();
  for (int i = 0; i < 3; ++i) {
    slots[i].set_visible(i == active);
    if (i == active) {
      slots[i].sync_layout(layout.map_slot_rect(), layout.dpi_scale());
    }
  }
};
layout.set_active_tab(0);
sync_all();
```

顶层 `WM_SIZE` / DPI 变更 → 更新 CEF browser 尺寸 + `sync_all()`。

- [x] **Step 4: 手动验证出帧**

Run: `build.bat cef` 后启动 `out\SmartGisCef.exe`。

Expected:

- HTML chrome 可见。
- 地图区原生 HWND 有帧或明确占位 + 不崩。
- 在地图 HWND 拖拽鼠标时，不要求 JS 收到 mousemove。

- [x] **Step 5: Include 纪律检查**

Run（在 `src/app/cef` 树内）确认无禁用头：

```bat
rg -n "gis_map|rd_renderdevice|SmtRenderDevice" src/app/cef
```

Expected: 无匹配。

---

### Task 3: `ChromeBridge` v1 + web IDE 语义（Catalog / Ambox / Tabs / Inspector）

**Files:**
- Create: `src/app/cef/chrome_bridge.h`, `chrome_bridge.cc`
- Create: `src/app/cef/chrome_bridge_test.cc` + GN test 目标（可选，推荐）
- Modify: `src/app/cef/cef_app.cc` / `cef_browser_host.cc`（注册 ProcessMessage 路由）
- Modify: `src/app/cef/web/index.html`, `css/shell.css`, `js/bridge.js`, 可增 `js/shell.js`
- Modify: `src/app/cef/BUILD.gn`

**Interfaces:**
- Consumes: Task 2 slots / `ViewHost::execute` / `activate` / `MapContents::CatalogCall`；`tool::Workspace` builtins
- Produces: `app::cef::ChromeBridge` 消息表（`api_version=1`）；JS `SmartGisBridge.post(msg)` / `onHost(msg)`

**写死：** 双向 **ProcessMessage JSON**（少 V8 绑定面）。字段：`api_version`、`type`、`request_id`（请求必填）、`view_id`。

- [x] **Step 1: 写失败单测（L0）锁定解析**

`chrome_bridge_test.cc`（控制台 exe 或并入既有测试风格）：

```cpp
#include "app/cef/chrome_bridge.h"

void test_parse_activate_tool() {
  const char* json =
      R"({"api_version":1,"type":"ActivateTool","request_id":"1",)"
      R"("view_id":1,"command_id":"selection.point"})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "parse ok");
  expect(msg.api_version == 1, "ver");
  expect(msg.type == app::cef::BridgeType::kActivateTool, "type");
  expect(msg.command_id == "selection.point", "cmd");
}

void test_reject_bad_version() {
  const char* json =
      R"({"api_version":99,"type":"Exit","request_id":"x","view_id":0})";
  app::cef::BridgeMessage msg;
  expect(app::cef::detail::parse_bridge_message(json, &msg), "parse structure");
  expect(!app::cef::detail::is_supported_api_version(msg.api_version), "reject");
}
```

Run: 先编译该测例，确认 FAIL / 链接缺口后实现。

- [x] **Step 2: 实现 `ChromeBridge` 宿主侧**

```cpp
namespace app {
namespace cef {

enum class BridgeType {
  kActivateTool,
  kCatalogOp,
  kSelectMapTab,
  kOpenFile,
  kLayoutSlot,
  kQueryState,
  kExit,
  kAck,
  kError,
  kStatus,
  kSelectionChanged,
  kExtentChanged,
  kLegendSnapshot,
  kCatalogDelta,
  kViewCursor,
  kReady,
};

struct BridgeMessage {
  int api_version = 0;
  BridgeType type = BridgeType::kError;
  std::string request_id;
  uint32_t view_id = 0;
  std::string command_id;
  std::string op_json;
  int tab_index = 0;
  std::string path;
  std::string slot_id;
  RectPx slot_rect;
  float dpi = 1.f;
  std::string query_what;
};

class ChromeBridge {
 public:
  void bind_browser(CefRefPtr<CefBrowser> browser);
  void set_handlers(/* LayoutHost*, CefMapSlot slots[3], MapContents* */);

  // Called from CefClient::OnProcessMessageReceived
  bool on_process_message(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefProcessMessage> message);

  void push_event(const BridgeMessage& msg);  // host → JS
  void notify_ready();

 private:
  void handle(const BridgeMessage& msg);
  void send_error(const std::string& request_id, int code, const char* text);
  std::string map_alias(std::string_view id);  // select→selection.point, pan→view.pan
};

}  // namespace cef
}  // namespace app
```

宿主行为（与 spec §6.2 一致）：

| type | 行为 |
| --- | --- |
| `ActivateTool` | `slots[active].view_host()->execute/activate(command_id)`；别名映射与 Views 一致 |
| `CatalogOp` | `session->CatalogCall(op_json.c_str())` |
| `SelectMapTab` | `layout.set_active_tab(index)` + slot 显隐 |
| `OpenFile` | 空 path → `IFileDialog`（self-test 可 suppress）；非空则打开 |
| `LayoutSlot` | `layout.set_map_slot_rect` + `sync_layout` |
| `QueryState` | 立即 `Status` / selection / legend 快照 |
| `Exit` | 退出消息循环 |
| 未知 `command_id` | `Error`；**不** `execute_legacy` 广播 leftover |
| `api_version!=1` | `Error`，拒绝写操作 |

热路径：地图 `mousemove` **不得**每帧进 JS；`ViewCursor` / `ExtentChanged` 宿主节流 10–20 Hz。

- [x] **Step 3: web IDE 壳**

`index.html` 结构（语义齐全，样式简洁即可）：

- `#menu-bar` — Open / Exit（发 bridge）
- `#catalog` — 图层树容器（由 `LegendSnapshot` / `CatalogDelta` 填充）
- `#tab-strip` — Map Edit | Data | 3D → `SelectMapTab`
- `#map-slot` — **仅占位**，不画地图
- `#ambox` — 按钮列表：至少
  - `selection.point` / `selection.clear`
  - `edit.append.point`
  - `view.pan` / `view.zoom_in` / `view.zoom_out`
  - `view3d.trackball`
  - 数据来自宿主下发的 builtins JSON（启动时 `QueryState` 或 `Ready` 附带），**不**链 `ui::views::AmboxView` / `plugin:host`
- `#inspector` — FeatureInfo / AttributeTable 两 tab 占位
- `#status-bar` — 显示 `Status` / `ViewCursor` 文案

`bridge.js`：

```javascript
const API_VERSION = 1;
function post(type, payload = {}) {
  const msg = Object.assign({ api_version: API_VERSION, type, request_id: String(Date.now()), view_id: 0 }, payload);
  // CefProcessMessage via injected native binding or cefQuery stand-in:
  window.chrome.webview /* wrong — do not use WebView2 */;
  window.smartgis_post(JSON.stringify(msg)); // provided by CefV8/native glue that only forwards strings
}
function onHost(raw) {
  const msg = JSON.parse(raw);
  if (msg.type === "Status") {
    document.getElementById("status-bar").textContent = msg.text || "";
  }
  // LayoutSlot: ResizeObserver on #map-slot → post LayoutSlot {x,y,w,h,dpi}
}
```

实现时：用 CEF `SendProcessMessage` 的渲染进程辅助（`CefApp` 在 renderer 注册短桥），**禁止**把 GIS 指针塞进 V8。

- [x] **Step 4: 首帧 `Ready`**

当：顶层 HWND 有效 + CEF main frame load end + 至少一个 `CefMapSlot` `create` 成功 → `ChromeBridge::notify_ready()`。

- [x] **Step 5: 手动验证命令路径**

Run: `out\SmartGisCef.exe`

Expected:

- 点击 Ambox「选择」→ 宿主激活 `selection.point`（可用日志 / Status）。
- 切换 Map|Data|3D → 仅活动 map HWND 可见（`IsWindowVisible`）。
- Catalog 开关发 `CatalogOp` 不崩。

---

### Task 4: `--self-test` 对齐 Views 退出码族

**Files:**
- Create: `src/app/cef/self_test.cc`（或并入 `main.cc`）
- Modify: `src/app/cef/main.cc`（`cmd_has_self_test` 分支）
- Modify: `src/app/cef/cef_browser_host.*` / `chrome_bridge.*`（load 超时、Ready 等待、self-test 抑 MessageBox）

**Interfaces:**
- Consumes: Task 2–3 全部宿主能力
- Produces: `SmartGisCef.exe --self-test` 退出码；`out/self-test-mark.txt` 步骤标记（仿 Views）

对齐参考：`src/app/views/main.cc` 的 `BrowserMain` self-test 顺序与码表；[`docs/build/ui-testing.md`](../../build/ui-testing.md)。

- [x] **Step 1: 实现 self-test 顺序**

```cpp
bool cmd_has_self_test() {
  const wchar_t* cmd = GetCommandLineW();
  return cmd && wcsstr(cmd, L"--self-test");
}

int run_self_test(app::cef::LayoutHost& layout,
                  app::cef::ChromeBridge& bridge,
                  app::cef::CefMapSlot slots[3]) {
  self_test_mark("show");
  pump_briefly(400);
  if (!layout.hwnd() || !IsWindow(layout.hwnd())) {
    return 2;
  }
  self_test_mark("hwnd-ok");

  if (!bridge.wait_ready(20000)) {
    return 42;  // CEF-only
  }
  self_test_mark("bridge-ready");

  // Map tab wait_ready when content attach active
  if (!slots[0].wait_ready(20000)) {
    return 3;
  }
  self_test_mark("map-ready");

  // Catalog / Ambox structure via QueryState (not pixels)
  if (!bridge.query_has_catalog_and_ambox()) {
    return 4;  // or 5/6 matching Views structure codes as documented in README
  }
  self_test_mark("catalog-ok");

  // Data tab: inactive map HWND must hide
  bridge.select_tab_for_test(1);
  pump_briefly(200);
  if (!slots[1].native_hwnd() || !IsWindow(slots[1].native_hwnd())) {
    return 7;
  }
  if (IsWindowVisible(slots[0].native_hwnd())) {
    return 36;
  }
  if (!IsWindowVisible(slots[1].native_hwnd())) {
    return 37;
  }
  if (!slots[1].wait_ready(20000)) {
    return 8;
  }
  self_test_mark("data-ready");

  bridge.select_tab_for_test(2);
  pump_briefly(400);
  if (!slots[2].native_hwnd() || !IsWindow(slots[2].native_hwnd())) {
    return 9;
  }
  if (!slots[2].wait_ready(20000)) {
    return 10;
  }
  self_test_mark("scene-ready");

  content::ViewHost* scene_host = slots[2].view_host();
  if (!scene_host || !scene_host->workspace()) {
    return 21;
  }
  if (!scene_host->activate("view3d.trackball")) {
    return 22;
  }
  // dispatch LDown / Move / LUp like Views main.cc (codes 23–24)
  // ...

  bridge.select_tab_for_test(0);
  content::ViewHost* host = slots[0].view_host();
  if (!host || !host->workspace() || !host->edits()) {
    return 11;
  }
  if (!host->execute("edit.append.point")) {
    return 12;
  }
  // interaction id draw.point → 13; dispatch → 14; can_undo → 15
  // Status text via bridge last Status snapshot: "Committed" → 16
  // selection.point → 17/18; selection.clear → 19/20
  // ...

  // map HWND vs slot rect tolerance (±2 px) → 33/34/35; zero/overlap → 43
  // layout smoke without ui::views::layout_check: assert slot rects positive → 30/31

  self_test_mark("pass");
  return 0;
}
```

CEF 独有码（禁止挪用 Views 语义）：

| 码 | 含义 |
| --- | --- |
| 40 | `CefInitialize` / browser 创建失败 |
| 41 | `web/` 未就绪 / 主 frame 加载失败 |
| 42 | Bridge `Ready` 超时 |
| 43 | Map slot 矩形无效（与 chrome 重叠判定失败或零面积） |

- [x] **Step 2: self-test 抑模态**

`OpenFile` / 错误 `MessageBox` 在 `--self-test` 下不弹模态（对齐 Views `set_message_box_suppressed_for_test` 精神；CEF 侧用标志位即可）。

- [x] **Step 3: 跑通 self-test**

Run:

```bat
build.bat cef
out\SmartGisCef.exe --self-test
echo EXIT=%ERRORLEVEL%
type out\self-test-mark.txt
```

Expected: `EXIT=0`；mark 含 `show` … `pass`。

失败时按码表修，禁止改 Views 已占用码的含义。

---

### Task 5: `exe_smoke` / 文档回写 + 进程逃生舱勾选

**Files:**
- Modify: `testing/e2e/exe_smoke.cc`（`kCases` 增补）
- Modify: `docs/build/ui-testing.md`（L1′ / L4 / 40+ 码）
- Modify: `docs/build/ui-shell-multiprocess.md`（§1 方案 1 = CEF 分区 HWND；澄清 §3.1(c)/§4.4 仅否定「CEF 冒充终局 Views」）
- Modify: `docs/build/ui-views-skia.md`（Rejected / alternate 一句指向 CEF spec）
- Modify: `docs/build/src-layout.md`（`app/{views,winui,cef}` 并列，最小改动）
- Modify: `BUILD.gn`（`group("e2e")` 在 `smt_build_cef && smt_has_cef` 时可选 deps）
- Modify: `build.bat`（`e2e` 是否置 `BUILD_CEF`：默认 **false**，避免无 pin 机器炸；文档写明手动 `smt_build_cef=true` 后编 CEF）
- Modify: `src/app/cef/README.md`（逃生舱说明）
- Modify: `docs/superpowers/specs/2026-09-14-app-cef-hwnd-host-design.md`（Plan 链接已指向本文件则核对 Status）

**Interfaces:**
- Consumes: Task 4 `--self-test` 退出 0
- Produces: L4 SKIP/FAIL 策略；as-built 文档与 spec 一致

- [x] **Step 1: `exe_smoke` 增加 CEF**

```cpp
const Case kCases[] = {
    {L"SmartGisRender.exe", nullptr, 45000, false},
    {L"SmartGisViews.exe", L"SmartGIS Views", 30000, false},
    {L"SmartGisWinui.exe", L"SmartGIS", 45000, false},
    {L"SmartGisCef.exe", L"SmartGIS CEF", 60000, false},
    {L"SmartGis.exe", L"SmartGis", 60000, true},
};
```

窗口标题与 `LayoutHost` / 创建窗时 `CreateWindowEx` 标题字符串保持一致（例如 `SmartGIS CEF`）。

缺二进制：默认 SKIP；`--require-all` 时 FAIL（现有逻辑已覆盖）。

- [x] **Step 2: 更新 `ui-testing.md`**

- L1′ 表增加 `SmartGisCef.exe --self-test`。
- L4 表增加 `SmartGisCef.exe` 行。
- 附录 CEF 40–43 码。
- 刷新「最后更新」为实施当日。

- [x] **Step 3: 回写 `ui-shell-multiprocess.md`**

按 spec §10：

- §1「方案 1」：已删除的 WebView2 → **CEF chrome + 分区 HWND + native map**（sibling HWND + `content` host）。
- §3.1 变体 (c) / §4.4「不选 CEF」：**仅**针对「用 CEF 冒充方案 3 / 终局 Views」；**不**否定第三壳产品路径。
- §0 底物不变。

- [x] **Step 4: 最小挂链其它 as-built**

- `ui-views-skia.md`：alternate 壳指向 CEF design spec。
- `src-layout.md`：`app/{views,winui,cef}`。
- 根 `README.md`：仅当模块表已列 WinUI/Views 入口时补一行 `build.bat cef`（若已有「不进 all」叙述则加半句）；无实质变化可跳过。

- [x] **Step 5: 逃生舱勾选（默认不启用）**

若实现期发现 `CefExecuteProcess` **误吞**本仓 `SmartGisCef.exe --type=renderer`（地图子进程）：

1. 地图 OOP 改为旁路 `SmartGisRender.exe`（`smt_build_render`）。
2. CEF PE **不再**承载 ContentMain 的 `--type=`；bridge / `MapContents` 仍在 browser 进程。
3. 在 `src/app/cef/README.md` 写明启用条件与标志。

**已启用（有 `SmartGisRender.exe` 时）：** `MapContents` 在宿主为 `SmartGisCef*` 且旁路 PE 存在时优先启动 `SmartGisRender.exe`；`wWinMain` 在 `--pipe=` 子进程跳过 `CefExecuteProcess`。无旁路 PE 时仍回退同 PE ContentMain。

- [x] **Step 6: 验收清单（对照 spec §9.3）**

- [x] 默认 `all` 不编 CEF（已验证）。`smt_build_cef=true` 且 pin 存在 → 产出 exe。
- [x] 启动可见 HTML：Catalog / Ambox / Map tabs / Inspector 语义齐全（`--self-test` bridge/catalog 路径）。
- [x] 地图原生 HWND 出帧（self-test marks：`map-frame-ok` / `scene-frame-ok`）；指针不经 JS。
- [x] command id：`selection.point` / `selection.clear` / `edit.append.point` / `view.pan` / `view3d.trackball` 可走通（self-test）。
- [x] `--self-test` 退出 0（有 CEF 的配置下）。
- [x] `src/app/cef` 源码无 `gis_map` / `SmtRenderDevice` include（README 提及除外）。
- [x] `exe_smoke` 对缺失 CEF SKIP（已验证）。

Run:

```bat
build.bat all
build.bat cef
out\SmartGisCef.exe --self-test
build.bat te
out\exe_smoke.exe
```

Expected: `all` 无 CEF；self-test 0；exe_smoke 在无/有 CEF 时 SKIP/PASS 符合策略。

---

## Self-review（对照 spec）

| Spec 节 | 覆盖任务 |
| --- | --- |
| Locked decisions / §11 否决 | Global Constraints + 各任务「明确不做」 |
| §3 分区 HWND | Task 1–2 |
| §4 目录与 GN / 不进 all | Task 1 |
| §5 组件 | Task 1–3 |
| §6 ChromeBridge | Task 3 |
| §7 CefExecuteProcess + ContentMain；逃生舱 | Task 1、Task 5 Step 5 |
| §8 错误处理 | Task 2–4（Status / 40+ / 不崩壳） |
| §9 测试与 40+ 码 | Task 4–5 |
| §10 文档关系 | Task 5 |
| §12 写死倾向（file://、ProcessMessage、三 slot、同 PE） | Task 1–3 |
| WinUI 不阻塞 | Global Constraints |
| 无 plugin:host 强依赖 | Task 3 Ambox builtins |

**Placeholder scan:** 无 TBD；CEF 具体版号由 Task 1 Step 1 写入 `third_party/cef/README.md`（实现时选定，非文档占位）。

**Type consistency:** `RectPx` / `BridgeMessage` / `CefMapSlot` / `ChromeBridge` / `LayoutHost` 贯穿 Task 1–4；退出码与 Views / 40+ 表一致。

---

**最后更新：** 2026-09-14
