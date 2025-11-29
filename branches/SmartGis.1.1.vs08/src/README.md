# SmartGIS Source Code Directory

SmartGIS 系统源代码目录，包含所有核心模块的源代码。

## 目录结构

本目录包含 SmartGIS 系统的所有源代码模块，按功能划分为以下几个主要类别：

### 核心基础模块

- **SmtCore**: 系统核心基础库，提供线程、内存、日志、插件等基础功能
- **SmtBaseLib**: 基础类型和工具库
- **SmtMathLib**: 数学运算库
- **SmtSysCore**: 系统核心库

### GIS 核心模块

- **SmtGeoCore**: 几何运算核心库，提供各种几何对象的定义和运算
- **SmtGisCore**: GIS 核心功能库，提供 GIS 数据模型和核心功能
- **SmtGisPrj**: 地图投影库

### UI 模块

- **SmtXViewCore**: 视图核心模块，提供基于 MFC CView 的地图视图基类
- **SmtXCatalogCore**: 目录树核心模块，提供数据源和图层目录树管理
- **SmtXAMBoxCore**: 功能箱核心模块，提供工具和功能的快捷访问界面
- **SmtGuiCore**: GUI 核心功能库
- **SmtMFCExCore**: MFC 扩展库

### 渲染模块

- **SmtRender**: 渲染抽象层，提供统一的渲染设备接口
- **SmtGdiRenderDevice**: GDI 渲染设备，基于 Windows GDI 的 2D 渲染
- **SmtGdiSimpleRenderDevice**: 简化 GDI 渲染设备
- **SmtGLRenderDevice**: OpenGL 渲染设备，基于 OpenGL 的 3D 渲染
- **SmtD3DRenderDevice**: Direct3D 渲染设备，基于 Direct3D 的 3D 渲染

### 3D 模块

- **Smt3DBaseLib**: 3D 基础库，提供 3D 场景管理和空间索引
- **Smt3DGeoCore**: 3D 几何对象库
- **Smt3DMathLib**: 3D 数学库
- **Smt3DMdLib**: 3D 模型库
- **Smt3DRenderer**: 3D 渲染器
- **Smt3DTerrain**: 地形渲染
- **Smt3DPointCloud**: 点云渲染

### 工具模块

- **SmtToolCore**: 工具核心模块，提供交互工具的基础框架
- **SmtGroupToolCore**: 组合工具库，支持编辑、浏览、分析等工具

### 数据源模块 (SDE)

- **SmtSDEDeviceMgr**: 数据源设备管理器
- **SmtSDEAdoDevice**: ADO 数据库数据源
- **SmtSDEMemDevice**: 内存数据源
- **SmtSDESmfDevice**: SMF 格式数据源
- **SmtSDEWSDevice**: Web 服务数据源

### WebGIS 模块

- **SmtMapServer**: 地图服务器
- **SmtMapService**: 地图服务
- **SmtMapClient**: 地图客户端
- **SmtConsoleMapServer**: 控制台地图服务器
- **SmtWinServiceMapServer**: Windows 服务地图服务器
- **SmtCgiWrapper**: CGI 包装器
- **SmtMapServerDeviceMgr**: 地图服务器设备管理器

### 网络模块

- **SmtNetCore**: 网络通信核心库

### 统计分析模块

- **SmtStaCore**: 统计分析核心库
- **SmtStaDiagram**: 统计图表库

### 其他功能模块

- **SmtDemCore**: DEM 核心库
- **SmtTinMesh**: TIN 三角网格库
- **SmtBAOrthGrid**: 包络正交网格库
- **SmtAdoCore**: ADO 数据库访问库
- **SmtAppCore**: 应用程序核心库

### 插件模块 (AM)

- **SmtAuxModule**: 辅助模块管理器
- **SmtAM3DModelCreater**: 3D 模型创建插件
- **SmtAMBAOGridCreater**: 包络网格创建插件
- **SmtAMDemCreater**: DEM 创建插件
- **SmtAMMapPrint**: 地图打印插件
- **SmtAMMapProject**: 地图投影插件
- **SmtAMMapServiceMgr**: 地图服务管理插件

### 主应用程序

- **SmartGis**: 主应用程序（MFC MDI）

## 模块依赖关系

```
SmartGis (主应用程序)
├── SmtXViewCore (视图核心)
│   ├── SmtCore (核心基础)
│   └── SmtRender (渲染抽象)
│       ├── SmtGdiRenderDevice (GDI 渲染)
│       ├── SmtGLRenderDevice (OpenGL 渲染)
│       └── SmtD3DRenderDevice (Direct3D 渲染)
├── SmtXCatalogCore (目录树)
│   ├── SmtCore
│   └── SmtGisCore
├── SmtXAMBoxCore (功能箱)
│   ├── SmtCore
│   └── SmtToolCore
├── SmtGisCore (GIS 核心)
│   ├── SmtCore
│   └── SmtGeoCore
└── Smt3DBaseLib (3D 基础)
    ├── SmtCore
    └── Smt3DRenderer
```

## 编译说明

所有模块使用 Visual Studio 2008 进行编译，项目文件位于 `vs2008` 目录下。

### 编译顺序

1. 首先编译基础模块：SmtCore, SmtBaseLib
2. 然后编译 GIS 核心模块：SmtGeoCore, SmtGisCore
3. 编译渲染模块：SmtRender, SmtGdiRenderDevice 等
4. 编译 UI 模块：SmtXViewCore, SmtXCatalogCore 等
5. 最后编译主应用程序：SmartGis

## 命名空间约定

各模块使用不同的命名空间：
- `Smt_Core`: 核心模块
- `Smt_Geo`: 几何模块
- `Smt_GIS`: GIS 模块
- `Smt_Rd`: 渲染模块
- `Smt_3DBase`: 3D 基础模块
- `Smt_XView`: 视图模块
- `Smt_SDE`: 数据源模块
- 等等

## 版本信息

- **版本**: SmartGIS 1.1
- **开发时间**: 2010-2013
- **开发环境**: Visual Studio 2008

## 更多信息

各模块的详细说明请参考各模块目录下的 README.md 文件。

---

# SmartGIS Source Code Directory

SmartGIS system source code directory, containing source code for all core modules.

## Directory Structure

This directory contains all source code modules of the SmartGIS system, divided into the following main categories by function:

### Core Base Modules

- **SmtCore**: System core base library, providing basic functions such as threads, memory, logging, plugins
- **SmtBaseLib**: Base types and utility library
- **SmtMathLib**: Math operations library
- **SmtSysCore**: System core library

### GIS Core Modules

- **SmtGeoCore**: Geometry computation core library, providing definitions and operations for various geometric objects
- **SmtGisCore**: GIS core functionality library, providing GIS data models and core functions
- **SmtGisPrj**: Map projection library

### UI Modules

- **SmtXViewCore**: View core module, providing map view base class based on MFC CView
- **SmtXCatalogCore**: Catalog tree core module, providing data source and layer catalog tree management
- **SmtXAMBoxCore**: Function box core module, providing quick access interface for tools and functions
- **SmtGuiCore**: GUI core functionality library
- **SmtMFCExCore**: MFC extension library

### Rendering Modules

- **SmtRender**: Rendering abstraction layer, providing unified render device interface
- **SmtGdiRenderDevice**: GDI render device, 2D rendering based on Windows GDI
- **SmtGdiSimpleRenderDevice**: Simplified GDI render device
- **SmtGLRenderDevice**: OpenGL render device, 3D rendering based on OpenGL
- **SmtD3DRenderDevice**: Direct3D render device, 3D rendering based on Direct3D

### 3D Modules

- **Smt3DBaseLib**: 3D base library, providing 3D scene management and spatial indexing
- **Smt3DGeoCore**: 3D geometry objects library
- **Smt3DMathLib**: 3D math library
- **Smt3DMdLib**: 3D model library
- **Smt3DRenderer**: 3D renderer
- **Smt3DTerrain**: Terrain rendering
- **Smt3DPointCloud**: Point cloud rendering

### Tool Modules

- **SmtToolCore**: Tool core module, providing base framework for interactive tools
- **SmtGroupToolCore**: Group tool library, supporting editing, browsing, analysis tools

### Data Source Modules (SDE)

- **SmtSDEDeviceMgr**: Data source device manager
- **SmtSDEAdoDevice**: ADO database data source
- **SmtSDEMemDevice**: Memory data source
- **SmtSDESmfDevice**: SMF format data source
- **SmtSDEWSDevice**: Web service data source

### WebGIS Modules

- **SmtMapServer**: Map server
- **SmtMapService**: Map service
- **SmtMapClient**: Map client
- **SmtConsoleMapServer**: Console map server
- **SmtWinServiceMapServer**: Windows service map server
- **SmtCgiWrapper**: CGI wrapper
- **SmtMapServerDeviceMgr**: Map server device manager

### Network Modules

- **SmtNetCore**: Network communication core library

### Statistical Analysis Modules

- **SmtStaCore**: Statistical analysis core library
- **SmtStaDiagram**: Statistical chart library

### Other Functional Modules

- **SmtDemCore**: DEM core library
- **SmtTinMesh**: TIN triangular mesh library
- **SmtBAOrthGrid**: Bounding box orthogonal grid library
- **SmtAdoCore**: ADO database access library
- **SmtAppCore**: Application core library

### Plugin Modules (AM)

- **SmtAuxModule**: Auxiliary module manager
- **SmtAM3DModelCreater**: 3D model creation plugin
- **SmtAMBAOGridCreater**: Bounding box grid creation plugin
- **SmtAMDemCreater**: DEM creation plugin
- **SmtAMMapPrint**: Map printing plugin
- **SmtAMMapProject**: Map projection plugin
- **SmtAMMapServiceMgr**: Map service management plugin

### Main Application

- **SmartGis**: Main application (MFC MDI)

## Module Dependencies

```
SmartGis (Main Application)
├── SmtXViewCore (View Core)
│   ├── SmtCore (Core Base)
│   └── SmtRender (Render Abstraction)
│       ├── SmtGdiRenderDevice (GDI Render)
│       ├── SmtGLRenderDevice (OpenGL Render)
│       └── SmtD3DRenderDevice (Direct3D Render)
├── SmtXCatalogCore (Catalog Tree)
│   ├── SmtCore
│   └── SmtGisCore
├── SmtXAMBoxCore (Function Box)
│   ├── SmtCore
│   └── SmtToolCore
├── SmtGisCore (GIS Core)
│   ├── SmtCore
│   └── SmtGeoCore
└── Smt3DBaseLib (3D Base)
    ├── SmtCore
    └── Smt3DRenderer
```

## Building

All modules are compiled using Visual Studio 2008, project files are located in the `vs2008` directory.

### Build Order

1. First compile base modules: SmtCore, SmtBaseLib
2. Then compile GIS core modules: SmtGeoCore, SmtGisCore
3. Compile rendering modules: SmtRender, SmtGdiRenderDevice, etc.
4. Compile UI modules: SmtXViewCore, SmtXCatalogCore, etc.
5. Finally compile main application: SmartGis

## Namespace Conventions

Each module uses different namespaces:
- `Smt_Core`: Core modules
- `Smt_Geo`: Geometry modules
- `Smt_GIS`: GIS modules
- `Smt_Rd`: Rendering modules
- `Smt_3DBase`: 3D base modules
- `Smt_XView`: View modules
- `Smt_SDE`: Data source modules
- etc.

## Version Information

- **Version**: SmartGIS 1.1
- **Development Period**: 2010-2013
- **Development Environment**: Visual Studio 2008

## More Information

For detailed information about each module, please refer to the README.md file in each module directory.

