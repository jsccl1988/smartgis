# SmtGisCore

SmartGIS GIS 核心功能库，提供 GIS 数据模型和核心 GIS 功能。

## 模块简介

SmtGisCore 是 SmartGIS 系统的 GIS 核心功能库，提供了 GIS 数据模型的定义和核心 GIS 功能的实现，包括图层、要素、属性等 GIS 数据结构的定义和管理。

## 主要功能

### GIS 数据模型

#### 1. 图层管理
- 图层（Layer）的定义和管理
- 图层的显示控制
- 图层的样式设置

#### 2. 要素管理
- 要素（Feature）的定义和管理
- 要素的几何信息和属性信息
- 要素的增删改查操作

#### 3. 属性管理
- 属性字段定义
- 属性值存储和访问
- 属性查询和过滤

#### 4. 空间索引
- 空间索引的创建和维护
- 空间查询优化

## 核心类

### 图层类
- 图层基类
- 矢量图层
- 栅格图层

### 要素类
- 要素基类
- 点要素
- 线要素
- 面要素

### 属性类
- 属性表定义
- 属性记录

## 使用说明

### 创建图层

```cpp
// 创建矢量图层
SmtVectorLayer* pLayer = new SmtVectorLayer();
pLayer->SetName("MyLayer");
```

### 添加要素

```cpp
// 创建要素
SmtFeature* pFeature = new SmtFeature();
pFeature->SetGeometry(pGeometry);
pFeature->SetAttribute("name", "Feature1");

// 添加到图层
pLayer->AddFeature(pFeature);
```

### 查询要素

```cpp
// 空间查询
SmtFeatureCollection* pFeatures = pLayer->QueryByGeometry(pQueryGeometry);

// 属性查询
SmtFeatureCollection* pFeatures = pLayer->QueryByAttribute("name", "Feature1");
```

## 依赖关系

- 依赖 **SmtCore**: 基础库支持
- 依赖 **SmtGeoCore**: 几何运算支持
- 依赖 **SmtBaseLib**: 基础类型定义

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013

---

# SmtGisCore

SmartGIS GIS core functionality library, providing GIS data models and core GIS functions.

## Module Overview

SmtGisCore is the GIS core functionality library of the SmartGIS system, providing definitions of GIS data models and implementation of core GIS functions, including definitions and management of GIS data structures such as layers, features, and attributes.

## Key Features

### GIS Data Model

#### 1. Layer Management
- Definition and management of layers
- Layer display control
- Layer style settings

#### 2. Feature Management
- Definition and management of features
- Feature geometry and attribute information
- Feature CRUD operations

#### 3. Attribute Management
- Attribute field definitions
- Attribute value storage and access
- Attribute query and filtering

#### 4. Spatial Index
- Creation and maintenance of spatial indexes
- Spatial query optimization

## Core Classes

### Layer Classes
- Layer base class
- Vector layer
- Raster layer

### Feature Classes
- Feature base class
- Point feature
- Line feature
- Surface feature

### Attribute Classes
- Attribute table definition
- Attribute records

## Usage

### Creating Layers

```cpp
// Create vector layer
SmtVectorLayer* pLayer = new SmtVectorLayer();
pLayer->SetName("MyLayer");
```

### Adding Features

```cpp
// Create feature
SmtFeature* pFeature = new SmtFeature();
pFeature->SetGeometry(pGeometry);
pFeature->SetAttribute("name", "Feature1");

// Add to layer
pLayer->AddFeature(pFeature);
```

### Querying Features

```cpp
// Spatial query
SmtFeatureCollection* pFeatures = pLayer->QueryByGeometry(pQueryGeometry);

// Attribute query
SmtFeatureCollection* pFeatures = pLayer->QueryByAttribute("name", "Feature1");
```

## Dependencies

- Depends on **SmtCore**: Base library support
- Depends on **SmtGeoCore**: Geometry computation support
- Depends on **SmtBaseLib**: Base type definitions

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013

