<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 外部 C# WinUI 宿主：MapView 控件 + SmartGisCs.exe

Status: active

外部 C# 进程当 chrome，经 **C ABI** 调 `content::MapContents`；地图仍在独立 GPU 进程画。可复用 `MapView` 控件与产品 exe 同契约。**不是**桌面壳终局（终局仍是 Views + Skia）。

## 背景

`src/app/{views,winui,cef}` 三套壳共用同一产品契约：MenuBar · Catalog · Ambox · Map Edit|Data|3D · Inspector · StatusBar，命令 id 走 `MapContents::ActivateTool` / `CatalogCall`。仓库无 .cs / CLR。WinUI 方案文档允许 C# island **仅可选**，且禁止把整个 chrome 改成 C# 当 GN 入口。

本设计满足「外部 C# 调 GIS」+「上限最高的 C# 工具包（WinUI 3）」+「控件与示例 exe 都要」+「与当前 app 同等产品能力 / UI」，同时遵守：GN/`build.bat` 仍是工程入口；session / IPC / present 仍是 C++。

## 非目标

- 不把 `SmartGisViews.exe` / 终局 chrome 改成 C#。
- 不把 `SmtMap` / `SmtGisCore` Load 进 C# 进程。
- 不以 `.csproj` / MSBuild 为仓库根入口。
- 本轮不做 MIDL + CsWinRT `runtimeclass`（C ABI 是稳定 FFI；WinRT 投影可后加，不改 C# chrome）。
- 不引入 Qt、C++/CLI、把 C# PE 当 `--type=gpu`。

## 进程与 FFI

| 进程 | 映像 | 职责 |
| --- | --- | --- |
| Chrome | 任意 C# WinUI 3 进程（产品为 `SmartGisCs.exe`） | XAML 壳 + `SmartGis.WinUI.MapView`；Load `smartgis_host[_d].dll` |
| GPU | **`SmartGisRender.exe`**（旁路，与 CEF 相同） | 现有 `--type=gpu`；C# PE **不能** ContentMain |

`MapContents::StartRenderProcess` 对 `SmartGisCef*` / `SmartGisCs*`（以及设置了 GPU exe 覆盖的嵌入方）优先旁路 `SmartGisRender.exe`。Views / WinUI C++ 仍同 PE 再拉起。

稳定 FFI 是 C（`sg_host_*`，`extern "C"`，`__cdecl`）。C# 用 `DllImport` / `NativeLibrary`。HWND island + software DIB present 与 `app/winui/MapHost` 同路径（`kSoftwareDib`，子窗口 blit `Latest()`）。

## 树与产物

| 路径 | 角色 |
| --- | --- |
| `src/app/cs/native/` | `smartgis_host` DLL：包 `MapContents` + HWND island |
| `src/app/cs/SmartGis.Host/` | P/Invoke + `MapSession` |
| `src/app/cs/SmartGis.WinUI/` | `MapView` 可复用控件 |
| `src/app/cs/SmartGisCs/` | 产品 chrome `SmartGisCs.exe` |

GN：`smt_build_cs` 默认 `false`。`build.bat cs` 置真，并编 `SmartGisRender.exe`。不进 `group("all")` / `src_all`。缺 `dotnet` 时 bat **明确失败**，不静默跳过。

## 产品 chrome 契约（与 WinUI C++ 对齐）

布局与命令 id 复制 `src/app/winui/main_window.cc`：

```
MenuBar (File: Open/Exit · View: Map/Data/3D · Tools: Select/Draw/Clear/Pan)
  Catalog 240 | Map Edit|Data|3D + MapView | Ambox 200
  Inspector: FeatureInfo | AttributeTable
  StatusBar
```

Workspace 工具：`selection.point` / `selection.clear` / `edit.append.point` / `view.pan` / `view.zoom_in` / `view.zoom_out` / `view3d.trackball`。Open → `CatalogCall({"op":"open","path":...})`。三视图保持打开（切 tab 不 `CloseView`）。

`--self-test`：IDE 区域齐全、子 HWND、OOP GPU、Map/Data/3D 出帧；退出码与 `SmartGisWinui.exe` 同号同义。

## 测试

| 层 | 入口 |
| --- | --- |
| L0 | `out/sg_host_test.exe`：create/destroy、catalog、假 HWND island |
| L1′ | `out/SmartGisCs.exe --self-test`（cwd=`out/`，需 `SmartGisRender.exe`） |

## 风险

- 首次 `dotnet restore` 需要 NuGet（WinAppSDK 1.7）。离线失败要打人话。
- C# 嵌入方必须能解析到 `smartgis_host*.dll` 与 `SmartGisRender.exe`（默认同目录）。
- 链接 `smartgis_host` 会带上 `content` → `tool/dispatch` → `sdb`；本机缺 GDAL 头（`ogrsf_frmts.h`）时 GN 链 DLL 失败。C# 工程可单独 `dotnet build`。
- Unpackaged WinUI 在 .NET SDK 10 上须关 PRI/MSIX（`EnableCoreMrtTooling=false`）。
