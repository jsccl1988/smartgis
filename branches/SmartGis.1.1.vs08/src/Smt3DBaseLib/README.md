# Smt3DBaseLib

SmartGIS 3D 基础库，提供 3D 场景管理、对象管理和空间索引功能。

## 模块简介

Smt3DBaseLib 是 SmartGIS 系统的 3D 基础库，提供了 3D 场景管理、3D 对象管理和空间索引功能。该模块是 3D 渲染系统的基础，支持场景的创建、更新、渲染，以及 3D 对象的添加、管理和空间查询。

## 主要功能

### 场景管理 (SmtScene)
- 3D 场景的创建和管理
- 场景相机管理
- 场景渲染
- 2D/3D 坐标转换
- 场景拾取

### 3D 对象管理
- 3D 对象的添加和删除
- 3D 对象查询
- 3D 对象变换（模型变换、世界变换）
- 3D 对象集合管理

### 空间索引
- 八叉树（Octree）空间索引
- 场景八叉树管理
- 顶点八叉树管理
- 空间查询优化

### 基础结构
- 3D 对象基类定义
- 场景基础结构
- 对象树节点

## 核心类

### SmtScene
3D 场景类，管理场景中的所有 3D 对象。

**主要接口：**
```cpp
// 场景设置
long Setup(void);
long Update(void);
long Render(void);

// 相机管理
SmtPerspCamera* GetSceneCamera();
void SetSceneCamera(SmtPerspCamera *pCamera);

// 3D 对象管理
void Add3DObject(Smt3DObject* p3DObject);
Smt3DObject* Get3DObject(int index);
void Remove3DObject(Smt3DObject* p3DObject);

// 坐标转换
long Transform2DTo3D(Vector3 &vOrg, Vector3 &vTar, const lPoint &point);
long Transform3DTo2D(const Vector3 &ver3D, lPoint &point);

// 对象变换
long TransModel3DObjects(Matrix& matTransform);
long TransWorld3DObjects(Matrix& matTransform);

// 拾取
long Pick3DObjects(const lPoint &point, vSmt3DObjectPtrs &v3DObjectPtrs);
```

### Smt3DObject
3D 对象基类，所有 3D 对象的基类。

### SmtSceneOctTree
场景八叉树，用于场景空间索引。

## 使用说明

### 创建场景

```cpp
// 创建场景
SmtScene* pScene = new SmtScene();

// 设置渲染设备
pScene->Set3DRenderDevice(pRenderDevice);

// 设置相机
SmtPerspCamera* pCamera = new SmtPerspCamera();
pScene->SetSceneCamera(pCamera);

// 初始化场景
pScene->Setup();

// 创建八叉树
pScene->CreateOctTreeSceneMgr();
```

### 添加 3D 对象

```cpp
// 创建 3D 对象
Smt3DObject* pObject = new Smt3DCube();

// 添加到场景
pScene->Add3DObject(pObject);

// 更新场景
pScene->Update();
```

### 场景渲染

```cpp
// 渲染场景
pScene->Render();
```

### 坐标转换

```cpp
// 2D 转 3D
Vector3 vOrg, vTar;
lPoint pt2D(100, 200);
pScene->Transform2DTo3D(vOrg, vTar, pt2D);

// 3D 转 2D
Vector3 v3D(0, 0, 0);
lPoint pt2D;
pScene->Transform3DTo2D(v3D, pt2D);
```

### 对象拾取

```cpp
// 拾取对象
lPoint ptMouse(100, 200);
vSmt3DObjectPtrs vObjects;
pScene->Pick3DObjects(ptMouse, vObjects);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 Smt3DMathLib**: 3D 数学库
- **依赖 Smt3DRenderer**: 3D 渲染器接口

## 命名空间

模块使用 `Smt_3DBase` 命名空间。

## 相关模块

- **Smt3DGeoCore**: 3D 几何对象
- **Smt3DMdLib**: 3D 模型库
- **Smt3DRenderer**: 3D 渲染器

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013
- **作者**: 陈春亮
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. 场景需要设置渲染设备后才能渲染
2. 场景更新应在对象变化后调用
3. 八叉树可以提高空间查询性能
4. 大量对象时建议使用八叉树进行优化

---

# Smt3DBaseLib

SmartGIS 3D base library, providing 3D scene management, object management, and spatial indexing functionality.

## Module Overview

Smt3DBaseLib is the 3D base library of the SmartGIS system, providing 3D scene management, 3D object management, and spatial indexing functionality. This module is the foundation of the 3D rendering system, supporting scene creation, update, rendering, and 3D object addition, management, and spatial queries.

## Key Features

### Scene Management (SmtScene)
- 3D scene creation and management
- Scene camera management
- Scene rendering
- 2D/3D coordinate transformation
- Scene picking

### 3D Object Management
- 3D object addition and removal
- 3D object queries
- 3D object transformation (model transformation, world transformation)
- 3D object collection management

### Spatial Indexing
- Octree spatial indexing
- Scene octree management
- Vertex octree management
- Spatial query optimization

### Base Structures
- 3D object base class definition
- Scene base structures
- Object tree nodes

## Core Classes

### SmtScene
3D scene class, managing all 3D objects in the scene.

**Main Interfaces:**
```cpp
// Scene setup
long Setup(void);
long Update(void);
long Render(void);

// Camera management
SmtPerspCamera* GetSceneCamera();
void SetSceneCamera(SmtPerspCamera *pCamera);

// 3D object management
void Add3DObject(Smt3DObject* p3DObject);
Smt3DObject* Get3DObject(int index);
void Remove3DObject(Smt3DObject* p3DObject);

// Coordinate transformation
long Transform2DTo3D(Vector3 &vOrg, Vector3 &vTar, const lPoint &point);
long Transform3DTo2D(const Vector3 &ver3D, lPoint &point);

// Object transformation
long TransModel3DObjects(Matrix& matTransform);
long TransWorld3DObjects(Matrix& matTransform);

// Picking
long Pick3DObjects(const lPoint &point, vSmt3DObjectPtrs &v3DObjectPtrs);
```

### Smt3DObject
3D object base class, base class for all 3D objects.

### SmtSceneOctTree
Scene octree for scene spatial indexing.

## Usage

### Creating Scene

```cpp
// Create scene
SmtScene* pScene = new SmtScene();

// Set render device
pScene->Set3DRenderDevice(pRenderDevice);

// Set camera
SmtPerspCamera* pCamera = new SmtPerspCamera();
pScene->SetSceneCamera(pCamera);

// Initialize scene
pScene->Setup();

// Create octree
pScene->CreateOctTreeSceneMgr();
```

### Adding 3D Objects

```cpp
// Create 3D object
Smt3DObject* pObject = new Smt3DCube();

// Add to scene
pScene->Add3DObject(pObject);

// Update scene
pScene->Update();
```

### Scene Rendering

```cpp
// Render scene
pScene->Render();
```

### Coordinate Transformation

```cpp
// 2D to 3D
Vector3 vOrg, vTar;
lPoint pt2D(100, 200);
pScene->Transform2DTo3D(vOrg, vTar, pt2D);

// 3D to 2D
Vector3 v3D(0, 0, 0);
lPoint pt2D;
pScene->Transform3DTo2D(v3D, pt2D);
```

### Object Picking

```cpp
// Pick objects
lPoint ptMouse(100, 200);
vSmt3DObjectPtrs vObjects;
pScene->Pick3DObjects(ptMouse, vObjects);
```

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on Smt3DMathLib**: 3D math library
- **Depends on Smt3DRenderer**: 3D renderer interface

## Namespace

The module uses the `Smt_3DBase` namespace.

## Related Modules

- **Smt3DGeoCore**: 3D geometry objects
- **Smt3DMdLib**: 3D model library
- **Smt3DRenderer**: 3D renderer

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
- **Author**: 陈春亮
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. Scene needs render device set before rendering
2. Scene update should be called after object changes
3. Octree can improve spatial query performance
4. Recommend using octree for optimization when there are many objects
