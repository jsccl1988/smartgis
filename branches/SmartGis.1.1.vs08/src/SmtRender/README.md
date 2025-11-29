# SmtRender

SmartGIS 渲染抽象层，提供统一的渲染设备接口和渲染器实现。

## 模块简介

SmtRender 是 SmartGIS 系统的渲染抽象层，提供了统一的渲染设备接口和渲染器实现。该模块采用 Bridge 模式，将渲染接口与具体实现分离，支持多种渲染后端（GDI、OpenGL、Direct3D 等）。

## 主要功能

### 渲染设备抽象 (SmtRenderDevice)
- 统一的渲染设备接口
- 视口和窗口口管理
- 渲染参数设置
- 多渲染层支持（Map、Dynamic、Quick、Direct）

### 渲染器 (SmtRenderer)
- 地图渲染器实现
- 要素渲染
- 样式渲染
- 渲染优化

### 渲染层管理
- **MRD_BL_MAP**: 地图层，持久化渲染
- **MRD_BL_DYNAMIC**: 动态层，临时渲染
- **MRD_BL_QUICK**: 快速层，快速渲染
- **MRD_BL_DIRECT**: 直接层，直接在 DC 上绘制

## 核心接口

### SmtRenderDevice

```cpp
// 初始化和销毁
virtual int Init(HWND hWnd, const char * logname) = 0;
virtual int Destroy(void) = 0;
virtual int Release(void) = 0;

// 视口管理
void SetViewport(const Viewport &viewport);
Viewport GetViewport(void) const;
void SetWindowport(const Windowport &windowport);
Windowport GetWindowport(void) const;

// 渲染控制
virtual int Lock() = 0;
virtual int Unlock() = 0;
virtual int BeginDraw(eRDBufferLayer layer) = 0;
virtual int EndDraw(eRDBufferLayer layer) = 0;
virtual int Refresh(eRDBufferLayer layer) = 0;
```

### SmtRenderer

```cpp
// 渲染地图
int RenderMap(SmtMap* pMap);

// 渲染要素
int RenderFeature(SmtFeature* pFeature);

// 渲染几何
int RenderGeometry(SmtGeometry* pGeometry);
```

## 使用说明

### 基本使用

```cpp
// 创建渲染设备
SmtRenderDevice* pDevice = new SmtGdiRenderDevice(hInst);

// 初始化
pDevice->Init(hWnd, "render.log");

// 设置视口
Viewport viewport;
viewport.x = 0;
viewport.y = 0;
viewport.width = 800;
viewport.height = 600;
pDevice->SetViewport(viewport);

// 创建渲染器
SmtRenderer* pRenderer = new SmtRenderer(pDevice);

// 渲染地图
pRenderer->RenderMap(pMap);
```

### 多渲染层使用

```cpp
// 渲染地图层（持久化）
pDevice->BeginDraw(MRD_BL_MAP);
pRenderer->RenderMap(pMap);
pDevice->EndDraw(MRD_BL_MAP);

// 渲染动态层（临时）
pDevice->BeginDraw(MRD_BL_DYNAMIC);
pRenderer->RenderGeometry(pTempGeometry);
pDevice->EndDraw(MRD_BL_DYNAMIC);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 SmtGisCore**: GIS 数据模型
- **依赖 SmtGeoCore**: 几何对象

## 命名空间

模块使用 `Smt_Rd` 命名空间。

## 实现模块

- **SmtGdiRenderDevice**: GDI 渲染设备实现
- **SmtGLRenderDevice**: OpenGL 渲染设备实现
- **SmtD3DRenderDevice**: Direct3D 渲染设备实现

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013
- **作者**: 陈春亮
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. 渲染设备需要正确初始化后才能使用
2. 渲染操作应在 Lock/Unlock 之间进行
3. 不同渲染层有不同的生命周期，需要注意管理
4. 直接层（DIRECT）的渲染内容在刷新后会消失

---

# SmtRender

SmartGIS rendering abstraction layer, providing unified render device interface and renderer implementation.

## Module Overview

SmtRender is the rendering abstraction layer of the SmartGIS system, providing a unified render device interface and renderer implementation. This module uses the Bridge pattern to separate rendering interfaces from specific implementations, supporting multiple rendering backends (GDI, OpenGL, Direct3D, etc.).

## Key Features

### Render Device Abstraction (SmtRenderDevice)
- Unified render device interface
- Viewport and windowport management
- Render parameter settings
- Multi-render layer support (Map, Dynamic, Quick, Direct)

### Renderer (SmtRenderer)
- Map renderer implementation
- Feature rendering
- Style rendering
- Render optimization

### Render Layer Management
- **MRD_BL_MAP**: Map layer, persistent rendering
- **MRD_BL_DYNAMIC**: Dynamic layer, temporary rendering
- **MRD_BL_QUICK**: Quick layer, fast rendering
- **MRD_BL_DIRECT**: Direct layer, direct drawing on DC

## Core Interfaces

### SmtRenderDevice

```cpp
// Initialization and destruction
virtual int Init(HWND hWnd, const char * logname) = 0;
virtual int Destroy(void) = 0;
virtual int Release(void) = 0;

// Viewport management
void SetViewport(const Viewport &viewport);
Viewport GetViewport(void) const;
void SetWindowport(const Windowport &windowport);
Windowport GetWindowport(void) const;

// Render control
virtual int Lock() = 0;
virtual int Unlock() = 0;
virtual int BeginDraw(eRDBufferLayer layer) = 0;
virtual int EndDraw(eRDBufferLayer layer) = 0;
virtual int Refresh(eRDBufferLayer layer) = 0;
```

### SmtRenderer

```cpp
// Render map
int RenderMap(SmtMap* pMap);

// Render feature
int RenderFeature(SmtFeature* pFeature);

// Render geometry
int RenderGeometry(SmtGeometry* pGeometry);
```

## Usage

### Basic Usage

```cpp
// Create render device
SmtRenderDevice* pDevice = new SmtGdiRenderDevice(hInst);

// Initialize
pDevice->Init(hWnd, "render.log");

// Set viewport
Viewport viewport;
viewport.x = 0;
viewport.y = 0;
viewport.width = 800;
viewport.height = 600;
pDevice->SetViewport(viewport);

// Create renderer
SmtRenderer* pRenderer = new SmtRenderer(pDevice);

// Render map
pRenderer->RenderMap(pMap);
```

### Multi-Layer Rendering

```cpp
// Render map layer (persistent)
pDevice->BeginDraw(MRD_BL_MAP);
pRenderer->RenderMap(pMap);
pDevice->EndDraw(MRD_BL_MAP);

// Render dynamic layer (temporary)
pDevice->BeginDraw(MRD_BL_DYNAMIC);
pRenderer->RenderGeometry(pTempGeometry);
pDevice->EndDraw(MRD_BL_DYNAMIC);
```

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on SmtGisCore**: GIS data model
- **Depends on SmtGeoCore**: Geometry objects

## Namespace

The module uses the `Smt_Rd` namespace.

## Implementation Modules

- **SmtGdiRenderDevice**: GDI render device implementation
- **SmtGLRenderDevice**: OpenGL render device implementation
- **SmtD3DRenderDevice**: Direct3D render device implementation

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
- **Author**: 陈春亮
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. Render device needs proper initialization before use
2. Render operations should be between Lock/Unlock
3. Different render layers have different lifecycles, need proper management
4. Direct layer (DIRECT) render content disappears after refresh

