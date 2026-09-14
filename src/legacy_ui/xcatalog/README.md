# SmtXCatalogCore

SmartGIS 目录树核心模块，提供数据源和图层目录树管理功能。

## 模块简介

SmtXCatalogCore 是 SmartGIS 系统的目录树核心模块，提供了数据源和图层目录树的管理功能。该模块用于在 GIS 应用程序中显示和管理数据源、图层等资源的树形结构。

## 主要功能

### 目录树管理
- 数据源目录树显示
- 图层目录树显示
- 目录树节点管理
- 目录树事件处理

### 数据源管理
- 数据源的添加和删除
- 数据源的展开和折叠
- 数据源属性显示

### 图层管理
- 图层的添加和删除
- 图层的显示控制
- 图层属性编辑
- 图层顺序调整

## 使用说明

### 基本使用

```cpp
// 创建目录树控件
SmtXCatalogTree* pTree = new SmtXCatalogTree();

// 初始化
pTree->Init();

// 添加数据源
pTree->AddDataSource(pDataSource);

// 添加图层
pTree->AddLayer(pLayer);
```

### 事件处理

```cpp
// 监听目录树事件
pTree->SetOnNodeSelectCallback(OnNodeSelect);
pTree->SetOnNodeDoubleClickCallback(OnNodeDoubleClick);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 SmtGisCore**: GIS 数据模型
- **依赖 MFC**: MFC 树形控件支持

## 命名空间

模块使用 `Smt_XCatalog` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013

## 注意事项

1. 目录树需要正确初始化后才能使用
2. 数据源和图层的变化需要及时更新目录树
3. 大量节点时需要注意性能优化

---

# SmtXCatalogCore

SmartGIS catalog tree core module, providing data source and layer catalog tree management functionality.

## Module Overview

SmtXCatalogCore is the catalog tree core module of the SmartGIS system, providing catalog tree management functionality for data sources and layers. This module is used to display and manage tree structures of resources such as data sources and layers in GIS applications.

## Key Features

### Catalog Tree Management
- Data source catalog tree display
- Layer catalog tree display
- Catalog tree node management
- Catalog tree event handling

### Data Source Management
- Add and remove data sources
- Expand and collapse data sources
- Data source property display

### Layer Management
- Add and remove layers
- Layer display control
- Layer property editing
- Layer order adjustment

## Usage

### Basic Usage

```cpp
// Create catalog tree control
SmtXCatalogTree* pTree = new SmtXCatalogTree();

// Initialize
pTree->Init();

// Add data source
pTree->AddDataSource(pDataSource);

// Add layer
pTree->AddLayer(pLayer);
```

### Event Handling

```cpp
// Listen to catalog tree events
pTree->SetOnNodeSelectCallback(OnNodeSelect);
pTree->SetOnNodeDoubleClickCallback(OnNodeDoubleClick);
```

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on SmtGisCore**: GIS data model
- **Depends on MFC**: MFC tree control support

## Namespace

The module uses the `Smt_XCatalog` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013

## Notes

1. Catalog tree needs proper initialization before use
2. Data source and layer changes need to update catalog tree in time
3. Performance optimization needed when there are many nodes

