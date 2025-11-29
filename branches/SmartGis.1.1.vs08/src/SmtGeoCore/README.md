# SmtGeoCore

SmartGIS 几何运算核心库，提供各种几何对象的定义和运算功能。

## 模块简介

SmtGeoCore 是 SmartGIS 系统的几何运算核心库，提供了完整的几何对象类型定义和几何运算功能，支持点、线、面等基本几何类型以及复合几何类型。

## 主要功能

### 几何对象类型

#### 1. 点几何 (geo_point.cpp)
- 单点对象
- 多点对象 (geo_multipoint.cpp)

#### 2. 线几何
- **geo_linestring.cpp**: 线串对象
- **geo_linearring.cpp**: 线性环对象
- **geo_multilinestring.cpp**: 多线串对象
- **geo_curve.cpp**: 曲线对象
- **geo_arc.cpp**: 弧线对象
- **geo_spline.cpp**: 样条曲线对象

#### 3. 面几何
- **geo_polygon.cpp**: 多边形对象
- **geo_multipolygon.cpp**: 多多边形对象
- **geo_fan.cpp**: 扇形对象

#### 4. 复合几何
- **geo_geometrycollection.cpp**: 几何集合对象

#### 5. 特殊几何
- **geo_tin.cpp**: TIN（三角不规则网络）对象
- **geo_grid.cpp**: 网格对象

### 核心接口

#### geo_geometry.h/cpp
几何对象的基类和通用接口定义。

#### geo_api.h/cpp
几何运算的 API 接口，提供各种几何运算函数。

## 几何运算功能

### 空间关系运算
- 相交（Intersection）
- 包含（Contains）
- 相离（Disjoint）
- 接触（Touches）
- 重叠（Overlaps）
- 内部（Within）
- 交叉（Crosses）

### 几何运算
- 缓冲区（Buffer）
- 并集（Union）
- 差集（Difference）
- 交集（Intersection）
- 对称差集（Symmetric Difference）

### 几何分析
- 面积计算
- 长度计算
- 距离计算
- 质心计算
- 边界计算

## 使用说明

### 创建几何对象

```cpp
// 创建点
SmtPoint* pPoint = new SmtPoint(x, y);

// 创建线串
SmtLineString* pLineString = new SmtLineString();
pLineString->AddPoint(x1, y1);
pLineString->AddPoint(x2, y2);

// 创建多边形
SmtPolygon* pPolygon = new SmtPolygon();
// 添加外环和内环
```

### 几何运算

```cpp
// 计算缓冲区
SmtGeometry* pBuffer = pGeometry->Buffer(distance);

// 计算并集
SmtGeometry* pUnion = pGeometry1->Union(pGeometry2);

// 计算交集
SmtGeometry* pIntersection = pGeometry1->Intersection(pGeometry2);
```

### 空间关系判断

```cpp
// 判断是否相交
bool bIntersects = pGeometry1->Intersects(pGeometry2);

// 判断是否包含
bool bContains = pGeometry1->Contains(pGeometry2);

// 判断是否在内部
bool bWithin = pGeometry1->Within(pGeometry2);
```

## 命名空间

模块使用 `Smt_Geo` 命名空间。

## 依赖关系

- 依赖 **SmtCore**: 基础库支持
- 依赖 **SmtBaseLib**: 基础类型定义

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013

## 注意事项

1. 几何对象使用后需要正确释放内存
2. 几何运算可能产生新的几何对象，需要管理其生命周期
3. 坐标系统需要保持一致
4. 复杂几何运算可能耗时较长，建议在后台线程执行

---

# SmtGeoCore

SmartGIS geometry computation core library, providing definitions and operations for various geometric objects.

## Module Overview

SmtGeoCore is the geometry computation core library of the SmartGIS system, providing complete geometric object type definitions and geometric computation functions, supporting basic geometric types such as points, lines, and surfaces, as well as composite geometric types.

## Key Features

### Geometric Object Types

#### 1. Point Geometry (geo_point.cpp)
- Single point object
- Multi-point object (geo_multipoint.cpp)

#### 2. Line Geometry
- **geo_linestring.cpp**: LineString object
- **geo_linearring.cpp**: LinearRing object
- **geo_multilinestring.cpp**: MultiLineString object
- **geo_curve.cpp**: Curve object
- **geo_arc.cpp**: Arc object
- **geo_spline.cpp**: Spline curve object

#### 3. Surface Geometry
- **geo_polygon.cpp**: Polygon object
- **geo_multipolygon.cpp**: MultiPolygon object
- **geo_fan.cpp**: Fan object

#### 4. Composite Geometry
- **geo_geometrycollection.cpp**: GeometryCollection object

#### 5. Special Geometry
- **geo_tin.cpp**: TIN (Triangulated Irregular Network) object
- **geo_grid.cpp**: Grid object

### Core Interfaces

#### geo_geometry.h/cpp
Base class and common interface definitions for geometric objects.

#### geo_api.h/cpp
API interfaces for geometric operations, providing various geometric computation functions.

## Geometric Operations

### Spatial Relationship Operations
- Intersection
- Contains
- Disjoint
- Touches
- Overlaps
- Within
- Crosses

### Geometric Operations
- Buffer
- Union
- Difference
- Intersection
- Symmetric Difference

### Geometric Analysis
- Area calculation
- Length calculation
- Distance calculation
- Centroid calculation
- Boundary calculation

## Usage

### Creating Geometric Objects

```cpp
// Create point
SmtPoint* pPoint = new SmtPoint(x, y);

// Create line string
SmtLineString* pLineString = new SmtLineString();
pLineString->AddPoint(x1, y1);
pLineString->AddPoint(x2, y2);

// Create polygon
SmtPolygon* pPolygon = new SmtPolygon();
// Add exterior and interior rings
```

### Geometric Operations

```cpp
// Calculate buffer
SmtGeometry* pBuffer = pGeometry->Buffer(distance);

// Calculate union
SmtGeometry* pUnion = pGeometry1->Union(pGeometry2);

// Calculate intersection
SmtGeometry* pIntersection = pGeometry1->Intersection(pGeometry2);
```

### Spatial Relationship Checks

```cpp
// Check if intersects
bool bIntersects = pGeometry1->Intersects(pGeometry2);

// Check if contains
bool bContains = pGeometry1->Contains(pGeometry2);

// Check if within
bool bWithin = pGeometry1->Within(pGeometry2);
```

## Namespace

The module uses the `Smt_Geo` namespace.

## Dependencies

- Depends on **SmtCore**: Base library support
- Depends on **SmtBaseLib**: Base type definitions

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013

## Notes

1. Geometric objects need proper memory deallocation after use
2. Geometric operations may create new geometric objects, requiring lifecycle management
3. Coordinate systems need to be consistent
4. Complex geometric operations may be time-consuming, recommend executing in background threads

