# SmartGIS

一个基于 MFC 开发的地理信息系统（GIS）桌面应用程序，支持 2D/3D 地图渲染、空间数据管理、WebGIS 服务等功能。

## 项目简介

SmartGIS 是一个功能完整的地理信息系统，开发于 2010-2013 年。系统采用模块化设计，模块耦合度较小，便于维护、扩充和跨平台移植。系统分为多个核心模块：UI 部件模块、渲染模块、交互工具模块、数据源模块、WebGIS 模块、插件系统模块等。

## 主要特性

### 核心模块

#### 1. UI 部件模块
- **SmartGis**: 主应用程序，基于 MFC MDI（多文档界面）架构
- **SmtGuiCore**: GUI 核心功能库
- **SmtMFCExCore**: MFC 扩展库
- **SmtXViewCore**: 视图核心模块
- **SmtXCatalogCore**: 目录树核心模块
- **SmtXAMBoxCore**: 功能箱核心模块

#### 2. 渲染模块
- **2D 渲染**:
  - `SmtGdiRenderDevice`: GDI 双缓冲绘图渲染设备
  - `SmtGdiSimpleRenderDevice`: 简化 GDI 渲染设备
  - 支持多线程绘图技术
- **3D 渲染**:
  - `SmtGLRenderDevice`: OpenGL 渲染设备
  - `SmtD3DRenderDevice`: Direct3D 渲染设备
  - `Smt3DRenderer`: 3D 渲染引擎，采用 Bridge 模式抽象渲染驱动接口
  - `Smt3DBaseLib`: 3D 基础库，包含场景管理和八叉树空间索引
  - `Smt3DGeoCore`: 3D 几何对象库（点、线、面、几何集合等）
  - `Smt3DMathLib`: 3D 数学库（向量、矩阵、射线、平面等）
  - `Smt3DMdLib`: 3D 模型库（立方体、球体、水面、指北针等）
  - `Smt3DTerrain`: 地形渲染
  - `Smt3DPointCloud`: 点云渲染

#### 3. 交互工具模块
- **SmtToolCore**: 工具核心库
- **SmtGroupToolCore**: 组合工具库，支持编辑、浏览、分析等工具
- 支持多种交互工具：编辑工具、浏览工具、分析工具等

#### 4. 数据源模块（SDE - Spatial Data Engine）
统一管理异构数据的存储，支持多种数据源：
- **SmtSDEAdoDevice**: ADO 数据库数据源
- **SmtSDEMemDevice**: 内存数据源
- **SmtSDESmfDevice**: SMF 格式数据源
- **SmtSDEWSDevice**: Web 服务数据源
- **SmtSDEDeviceMgr**: 数据源设备管理器

#### 5. WebGIS 模块
基于线程池技术实现多线程并发地图服务：
- **SmtMapServer**: 地图服务器
- **SmtMapService**: 地图服务
- **SmtMapClient**: 地图客户端
- **SmtConsoleMapServer**: 控制台地图服务器
- **SmtWinServiceMapServer**: Windows 服务地图服务器
- **SmtCgiWrapper**: CGI 包装器
- 支持 OGC 标准地图服务访问接口（WMS、WTS）
- 支持瓦片切图功能
- 客户端使用 JavaScript（OpenLayers）进行测试

#### 6. 插件系统模块
可扩展的第三方功能模块系统：
- **SmtAuxModule**: 辅助模块管理器，提供插件注册和管理机制
- **SmtAM3DModelCreater**: 3D 模型创建插件
- **SmtAMBAOGridCreater**: 包络网格创建插件
- **SmtAMDemCreater**: DEM 创建插件
- **SmtAMMapPrint**: 地图打印插件
- **SmtAMMapProject**: 地图投影插件
- **SmtAMMapServiceMgr**: 地图服务管理插件

#### 7. GIS 核心模块
- **SmtCore**: 系统核心功能库
- **SmtGeoCore**: 几何运算核心库
- **SmtGisCore**: GIS 核心功能库
- **SmtGisPrj**: 地图投影库
- **SmtMathLib**: 数学运算库
- **SmtBaseLib**: 基础库

#### 8. 其他功能模块
- **SmtRender**: 渲染抽象层
- **SmtNetCore**: 网络通信核心库
- **SmtStaCore**: 统计分析核心库
- **SmtStaDiagram**: 统计图表库
- **SmtDemCore**: DEM 核心库
- **SmtTinMesh**: TIN 三角网格库
- **SmtBAOrthGrid**: 包络正交网格库
- **SmtAdoCore**: ADO 数据库访问库
- **SmtAppCore**: 应用程序核心库
- **SmtSysCore**: 系统核心库

## 技术架构

### 设计模式
- **Bridge 模式**: 3D 渲染引擎采用 Bridge 模式将渲染接口与实现分离
- **模块化设计**: 系统模块耦合度小，便于维护和扩展
- **插件架构**: 支持第三方功能模块扩展

### 技术特点
- **多线程绘图**: 2D 视图采用 GDI 双缓冲绘图和多线程绘图技术
- **多线程并发服务**: WebGIS 模块基于线程池技术实现多线程并发地图服务
- **跨进程通信**: 地图服务和 Web 服务通过 Socket 进行跨进程通信
- **空间索引**: 3D 场景使用八叉树进行空间管理

### 开发技术栈
- **UI 框架**: MFC (Microsoft Foundation Classes)
- **3D 渲染**: OpenGL, Direct3D
- **2D 渲染**: GDI (Graphics Device Interface)
- **数据库**: ADO (ActiveX Data Objects)
- **Web 服务**: CGI, Socket 通信
- **开发环境**: Visual Studio 2008

## 项目结构

```
smartgis/
├── branches/
│   └── SmartGis.1.1.vs08/          # VS2008 版本源代码
│       ├── bin/                     # 编译输出目录
│       ├── src/                     # 源代码目录
│       │   ├── SmartGis/           # 主应用程序（MFC MDI）
│       │   ├── Smt3D*/             # 3D 相关模块
│       │   ├── SmtCore/             # 核心模块
│       │   ├── SmtGdi*/            # GDI 渲染设备
│       │   ├── SmtGL*/             # OpenGL 渲染设备
│       │   ├── SmtD3D*/            # Direct3D 渲染设备
│       │   ├── SmtSDE*/            # 数据源模块
│       │   ├── SmtMap*/            # WebGIS 模块
│       │   ├── SmtTool*/           # 工具模块
│       │   ├── SmtAuxModule/       # 插件系统
│       │   └── SmtAM*/             # 各种插件模块
│       └── vs2008/                  # Visual Studio 2008 解决方案
│           └── SmartGIS.sln         # 主解决方案文件
├── doc/                             # 文档目录
└── README.md                        # 项目说明文档
```

## 编译说明

### 环境要求
- **操作系统**: Windows
- **开发环境**: Visual Studio 2008 或更高版本
- **依赖库**: MFC 库（随 VS 安装）

### 编译步骤

1. **获取源代码**
   - 将项目代码解压至 `branches/SmartGis.1.1.vs08` 目录

2. **打开解决方案**
   - 使用 Visual Studio 2008 打开 `branches/SmartGis.1.1.vs08/vs2008/SmartGIS.sln`

3. **编译项目**
   - 在 Visual Studio 中选择相应的配置（Debug/Release）
   - 执行"生成解决方案"命令

4. **运行程序**
   - 编译完成后，可执行文件位于 `bin` 目录下
   - 直接运行 `SmartGIS.exe` 即可

## 功能说明

### 已实现功能
- ✅ 2D 地图渲染（GDI 双缓冲、多线程绘图）
- ✅ 3D 地图渲染（OpenGL/Direct3D）
- ✅ 空间数据管理（多种数据源支持）
- ✅ 地图编辑工具
- ✅ 地图浏览工具
- ✅ 统计分析功能
- ✅ Web 地图发布（WMS、WTS 服务）
- ✅ 瓦片切图功能
- ✅ 插件系统
- ✅ 地图打印功能
- ✅ 地图投影功能

### WebGIS 功能说明
SmartGIS Web 地图发布功能已经能够实现：
- Web 地图的发布
- 瓦片切图
- WMS 和 WTS 服务

**注意**: Apache Web 服务器配置目前仍未实现定制，需要用户自己修改配置文件或配置 IIS。因此 Web 地图部分功能可暂时不管，但源码部分仍然提供。

## 版本信息

- **开发时间**: 2010-2013
- **当前版本**: SmartGIS 1.1
- **开发环境**: Visual Studio 2008

## 技术文档

更多详细文档请参考 `doc/` 目录下的文档文件。

## 许可证

本项目为内部项目，版权归原作者所有。

## 开发说明

### 模块设计原则
- 模块耦合度小，便于维护和扩展
- 采用面向对象设计思想
- 支持跨平台移植（核心库用 C++ 实现）

### 扩展开发
- 可以通过插件系统扩展第三方功能模块
- 可以添加新的数据源设备
- 可以添加新的渲染设备
- 可以添加新的工具模块

---

# SmartGIS

A Geographic Information System (GIS) desktop application developed with MFC, supporting 2D/3D map rendering, spatial data management, WebGIS services, and more.

## Overview

SmartGIS is a comprehensive GIS system developed from 2010 to 2013. The system features a modular design with low module coupling, making it easy to maintain, extend, and port across platforms. The system consists of multiple core modules: UI components, rendering modules, interactive tools, data source modules, WebGIS modules, and plugin system modules.

## Key Features

### Core Modules

#### 1. UI Components Module
- **SmartGis**: Main application based on MFC MDI (Multiple Document Interface) architecture
- **SmtGuiCore**: GUI core functionality library
- **SmtMFCExCore**: MFC extension library
- **SmtXViewCore**: View core module
- **SmtXCatalogCore**: Catalog tree core module
- **SmtXAMBoxCore**: Function box core module

#### 2. Rendering Module
- **2D Rendering**:
  - `SmtGdiRenderDevice`: GDI double-buffered rendering device
  - `SmtGdiSimpleRenderDevice`: Simplified GDI rendering device
  - Supports multi-threaded drawing
- **3D Rendering**:
  - `SmtGLRenderDevice`: OpenGL rendering device
  - `SmtD3DRenderDevice`: Direct3D rendering device
  - `Smt3DRenderer`: 3D rendering engine using Bridge pattern to abstract rendering driver interfaces
  - `Smt3DBaseLib`: 3D base library with scene management and octree spatial indexing
  - `Smt3DGeoCore`: 3D geometry objects library (points, lines, surfaces, geometry collections)
  - `Smt3DMathLib`: 3D math library (vectors, matrices, rays, planes)
  - `Smt3DMdLib`: 3D model library (cubes, spheres, water surfaces, north indicators)
  - `Smt3DTerrain`: Terrain rendering
  - `Smt3DPointCloud`: Point cloud rendering

#### 3. Interactive Tools Module
- **SmtToolCore**: Tool core library
- **SmtGroupToolCore**: Group tool library supporting editing, browsing, analysis tools
- Supports various interactive tools: editing, browsing, analysis

#### 4. Data Source Module (SDE - Spatial Data Engine)
Unified management of heterogeneous data storage, supporting multiple data sources:
- **SmtSDEAdoDevice**: ADO database data source
- **SmtSDEMemDevice**: Memory data source
- **SmtSDESmfDevice**: SMF format data source
- **SmtSDEWSDevice**: Web service data source
- **SmtSDEDeviceMgr**: Data source device manager

#### 5. WebGIS Module
Multi-threaded concurrent map services based on thread pool technology:
- **SmtMapServer**: Map server
- **SmtMapService**: Map service
- **SmtMapClient**: Map client
- **SmtConsoleMapServer**: Console map server
- **SmtWinServiceMapServer**: Windows service map server
- **SmtCgiWrapper**: CGI wrapper
- Supports OGC standard map service access interfaces (WMS, WTS)
- Supports tile cutting functionality
- Client uses JavaScript (OpenLayers) for testing

#### 6. Plugin System Module
Extensible third-party functionality module system:
- **SmtAuxModule**: Auxiliary module manager providing plugin registration and management
- **SmtAM3DModelCreater**: 3D model creation plugin
- **SmtAMBAOGridCreater**: Bounding box grid creation plugin
- **SmtAMDemCreater**: DEM creation plugin
- **SmtAMMapPrint**: Map printing plugin
- **SmtAMMapProject**: Map projection plugin
- **SmtAMMapServiceMgr**: Map service management plugin

#### 7. GIS Core Module
- **SmtCore**: System core functionality library
- **SmtGeoCore**: Geometry operations core library
- **SmtGisCore**: GIS core functionality library
- **SmtGisPrj**: Map projection library
- **SmtMathLib**: Math operations library
- **SmtBaseLib**: Base library

#### 8. Other Functional Modules
- **SmtRender**: Rendering abstraction layer
- **SmtNetCore**: Network communication core library
- **SmtStaCore**: Statistical analysis core library
- **SmtStaDiagram**: Statistical chart library
- **SmtDemCore**: DEM core library
- **SmtTinMesh**: TIN triangular mesh library
- **SmtBAOrthGrid**: Bounding box orthogonal grid library
- **SmtAdoCore**: ADO database access library
- **SmtAppCore**: Application core library
- **SmtSysCore**: System core library

## Technical Architecture

### Design Patterns
- **Bridge Pattern**: 3D rendering engine uses Bridge pattern to separate rendering interface from implementation
- **Modular Design**: Low module coupling for easy maintenance and extension
- **Plugin Architecture**: Supports third-party functionality module extensions

### Technical Features
- **Multi-threaded Drawing**: 2D views use GDI double-buffered drawing and multi-threaded drawing techniques
- **Multi-threaded Concurrent Services**: WebGIS module implements multi-threaded concurrent map services based on thread pool technology
- **Inter-process Communication**: Map services and Web services communicate via Socket
- **Spatial Indexing**: 3D scenes use octree for spatial management

### Technology Stack
- **UI Framework**: MFC (Microsoft Foundation Classes)
- **3D Rendering**: OpenGL, Direct3D
- **2D Rendering**: GDI (Graphics Device Interface)
- **Database**: ADO (ActiveX Data Objects)
- **Web Services**: CGI, Socket communication
- **Development Environment**: Visual Studio 2008

## Project Structure

```
smartgis/
├── branches/
│   └── SmartGis.1.1.vs08/          # VS2008 version source code
│       ├── bin/                     # Build output directory
│       ├── src/                     # Source code directory
│       │   ├── SmartGis/           # Main application (MFC MDI)
│       │   ├── Smt3D*/             # 3D related modules
│       │   ├── SmtCore/             # Core modules
│       │   ├── SmtGdi*/            # GDI rendering devices
│       │   ├── SmtGL*/             # OpenGL rendering devices
│       │   ├── SmtD3D*/            # Direct3D rendering devices
│       │   ├── SmtSDE*/            # Data source modules
│       │   ├── SmtMap*/            # WebGIS modules
│       │   ├── SmtTool*/           # Tool modules
│       │   ├── SmtAuxModule/       # Plugin system
│       │   └── SmtAM*/             # Various plugin modules
│       └── vs2008/                  # Visual Studio 2008 solution
│           └── SmartGIS.sln         # Main solution file
├── doc/                             # Documentation directory
└── README.md                        # Project documentation
```

## Building

### Requirements
- **OS**: Windows
- **Development Environment**: Visual Studio 2008 or higher
- **Dependencies**: MFC library (included with VS)

### Build Steps

1. **Get Source Code**
   - Extract project code to `branches/SmartGis.1.1.vs08` directory

2. **Open Solution**
   - Open `branches/SmartGis.1.1.vs08/vs2008/SmartGIS.sln` with Visual Studio 2008

3. **Build Project**
   - Select appropriate configuration (Debug/Release) in Visual Studio
   - Execute "Build Solution" command

4. **Run Application**
   - After compilation, executable is located in `bin` directory
   - Run `SmartGIS.exe` directly

## Features

### Implemented Features
- ✅ 2D map rendering (GDI double-buffering, multi-threaded drawing)
- ✅ 3D map rendering (OpenGL/Direct3D)
- ✅ Spatial data management (multiple data source support)
- ✅ Map editing tools
- ✅ Map browsing tools
- ✅ Statistical analysis functionality
- ✅ Web map publishing (WMS, WTS services)
- ✅ Tile cutting functionality
- ✅ Plugin system
- ✅ Map printing functionality
- ✅ Map projection functionality

### WebGIS Features
SmartGIS Web map publishing functionality supports:
- Web map publishing
- Tile cutting
- WMS and WTS services

**Note**: Apache Web server configuration is not yet customized and requires users to modify configuration files or configure IIS. Therefore, Web map functionality can be ignored for now, but source code is still provided.

## Version Information

- **Development Period**: 2010-2013
- **Current Version**: SmartGIS 1.1
- **Development Environment**: Visual Studio 2008

## Documentation

For more detailed documentation, please refer to files in the `doc/` directory.

## License

This is an internal project, copyright belongs to the original author.

## Development Notes

### Module Design Principles
- Low module coupling for easy maintenance and extension
- Object-oriented design philosophy
- Cross-platform portability (core libraries implemented in C++)

### Extension Development
- Extend third-party functionality modules through plugin system
- Add new data source devices
- Add new rendering devices
- Add new tool modules
