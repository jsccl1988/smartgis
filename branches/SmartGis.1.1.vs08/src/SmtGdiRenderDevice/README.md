# SmtGdiRenderDevice

SmartGIS GDI 渲染设备，提供基于 Windows GDI 的 2D 地图渲染实现。

## 模块简介

SmtGdiRenderDevice 是 SmartGIS 系统的 GDI 渲染设备实现，提供了基于 Windows GDI（Graphics Device Interface）的 2D 地图渲染功能。该模块实现了 `SmtRenderDevice` 接口，支持双缓冲绘图和多线程绘图技术。

## 主要功能

### GDI 渲染实现
- 基于 Windows GDI 的渲染
- 双缓冲绘图支持
- 多线程绘图支持
- 多种绘图模式支持

### 渲染层管理
- Map 层：持久化地图渲染
- Dynamic 层：动态临时渲染
- Quick 层：快速渲染
- Direct 层：直接 DC 绘制

### 渲染优化
- 视口裁剪
- 渲染缓存
- 增量渲染

## 核心特性

### 双缓冲绘图
使用双缓冲技术避免闪烁，提供流畅的渲染效果。

### 多线程绘图
支持多线程绘图，提高渲染性能。

### 渲染缓冲区
管理多个渲染缓冲区，支持不同渲染层的独立管理。

## 使用说明

### 基本使用

```cpp
// 创建 GDI 渲染设备
SmtGdiRenderDevice* pDevice = new SmtGdiRenderDevice(hInst);

// 初始化
pDevice->Init(hWnd, "gdi_render.log");

// 设置视口
Viewport viewport;
viewport.x = 0;
viewport.y = 0;
viewport.width = 800;
viewport.height = 600;
pDevice->SetViewport(viewport);

// 开始绘制
pDevice->Lock();
pDevice->BeginDraw(MRD_BL_MAP);

// 绘制地图
// ...

// 结束绘制
pDevice->EndDraw(MRD_BL_MAP);
pDevice->Unlock();

// 刷新显示
pDevice->Refresh(MRD_BL_MAP);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 SmtRender**: 渲染抽象层
- **依赖 Windows GDI**: Windows 图形设备接口

## 命名空间

模块使用 `Smt_Rd` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013
- **作者**: 陈春亮
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. GDI 渲染设备需要有效的窗口句柄
2. 渲染操作应在 Lock/Unlock 之间进行
3. 双缓冲需要正确管理缓冲区
4. 多线程绘图需要注意线程安全

---

# SmtGdiRenderDevice

SmartGIS GDI render device, providing 2D map rendering implementation based on Windows GDI.

## Module Overview

SmtGdiRenderDevice is the GDI render device implementation of the SmartGIS system, providing 2D map rendering functionality based on Windows GDI (Graphics Device Interface). This module implements the `SmtRenderDevice` interface, supporting double-buffered drawing and multi-threaded drawing techniques.

## Key Features

### GDI Rendering Implementation
- Rendering based on Windows GDI
- Double-buffered drawing support
- Multi-threaded drawing support
- Multiple drawing mode support

### Render Layer Management
- Map layer: Persistent map rendering
- Dynamic layer: Dynamic temporary rendering
- Quick layer: Fast rendering
- Direct layer: Direct DC drawing

### Render Optimization
- Viewport clipping
- Render caching
- Incremental rendering

## Core Features

### Double-Buffered Drawing
Uses double-buffering technique to avoid flickering and provide smooth rendering.

### Multi-Threaded Drawing
Supports multi-threaded drawing to improve rendering performance.

### Render Buffers
Manages multiple render buffers, supporting independent management of different render layers.

## Usage

### Basic Usage

```cpp
// Create GDI render device
SmtGdiRenderDevice* pDevice = new SmtGdiRenderDevice(hInst);

// Initialize
pDevice->Init(hWnd, "gdi_render.log");

// Set viewport
Viewport viewport;
viewport.x = 0;
viewport.y = 0;
viewport.width = 800;
viewport.height = 600;
pDevice->SetViewport(viewport);

// Begin drawing
pDevice->Lock();
pDevice->BeginDraw(MRD_BL_MAP);

// Draw map
// ...

// End drawing
pDevice->EndDraw(MRD_BL_MAP);
pDevice->Unlock();

// Refresh display
pDevice->Refresh(MRD_BL_MAP);
```

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on SmtRender**: Rendering abstraction layer
- **Depends on Windows GDI**: Windows Graphics Device Interface

## Namespace

The module uses the `Smt_Rd` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
- **Author**: 陈春亮
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. GDI render device needs valid window handle
2. Render operations should be between Lock/Unlock
3. Double-buffering needs proper buffer management
4. Multi-threaded drawing needs thread safety attention
