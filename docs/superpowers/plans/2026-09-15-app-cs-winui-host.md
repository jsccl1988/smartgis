<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 外部 C# WinUI 宿主 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 `smartgis_host` C ABI + `SmartGis.WinUI.MapView` + 与现有 app 同契约的 `SmartGisCs.exe`。

**Architecture:** C# 进程是 chrome；native DLL 包 `MapContents` 与 HWND+DIB island；GPU 必须是旁路 `SmartGisRender.exe`。

**Tech Stack:** C++23 GN DLL、.NET 8 WinUI 3 unpackaged、WinAppSDK 1.7 NuGet、P/Invoke。

## Global Constraints

- 工程入口只有 `build.bat` / GN；`.csproj` 不是仓库根。
- `smt_build_cs` 默认 false；不进 `all` / `src_all`。
- Chrome 不准 include `gis_map.h` / `rd_renderdevice.h`。
- 新注释英文；本 plan/spec 中文。
- 函数 `snake_case`（C ABI 用 `sg_host_*` 前缀）。
- 版权年 2026。

---

### Task 1: Native C ABI + L0 测试

**Files:**
- Create: `src/app/cs/native/sg_host.h`
- Create: `src/app/cs/native/sg_host.cc`
- Create: `src/app/cs/native/sg_host_test.cc`
- Create: `src/app/cs/cs.gni`
- Create: `src/app/cs/BUILD.gn`
- Modify: `src/content/map_contents.cc`（`SmartGisCs*` + `SetGpuExeOverride`）
- Modify: `src/content/public/map_contents.h`

**Produces:** `sg_host_*`；`out/smartgis_host_d.dll`；`out/sg_host_test.exe`

- [x] L0 测试与 native 实现同轮落地（create/destroy、假 HWND、catalog）
- [x] `MapContents::SetGpuExeOverride` + resolve_render_exe 识别 Cs/Cef

### Task 2: C# MapSession + MapView + 产品壳

**Files:** `src/app/cs/SmartGis.Host/`、`SmartGis.WinUI/`、`SmartGisCs/`、`dotnet_build.py`

**Produces:** `out/SmartGisCs.exe`；可复用 `SmartGis.WinUI.dll`

- [x] P/Invoke 包装 `sg_host_*`
- [x] `MapView`：Loaded 时 attach HWND，SizeChanged 调 `sg_host_sync_layout`
- [x] MainWindow 复制 WinUI C++ IDE 区域与命令 id
- [x] `--self-test`

### Task 3: GN / bat / 文档

- [x] `build.bat cs`、根 `group("cs")`、README / ui-testing / src-layout
