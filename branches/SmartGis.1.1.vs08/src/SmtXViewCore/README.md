# SmtXViewCore

SmartGIS 视图核心模块，提供基于 MFC CView 的地图视图基类。

## 模块简介

SmtXViewCore 是 SmartGIS 系统的视图核心模块，提供了基于 MFC CView 的地图视图基类 `SmtXView`。该模块封装了地图视图的基本功能，包括窗口绑定、消息处理、渲染初始化、工具创建等。

## 主要功能

### 核心类

#### SmtXView
继承自 `CView` 和 `SmtListener`，是地图视图的基类。

**主要功能：**
- 窗口绑定：支持绑定到窗口句柄或对话框控件
- 消息处理：处理 Windows 消息和自定义消息
- 渲染初始化：创建渲染设备和渲染器
- 工具管理：创建和管理交互工具
- 菜单管理：创建主菜单和上下文菜单
- 生命周期管理：视图的创建和销毁

### 主要接口

#### 窗口绑定
```cpp
long BindWind(HWND hWnd);                    // 绑定到窗口句柄
long BindDlgItem(CDialog *pDlg, UINT nItemID); // 绑定到对话框控件
long UnbindWind(void);                       // 解除绑定
```

#### 初始化接口
```cpp
virtual bool InitCreate(void);               // 初始化创建
virtual bool EndDestory(void);               // 结束销毁
virtual bool CreateMainMenu(void);           // 创建主菜单
virtual bool CreateContexMenu(void);         // 创建上下文菜单
virtual bool CreateRender(void);             // 创建渲染器
virtual bool CreateTools(void);              // 创建工具
```

#### 消息处理
```cpp
virtual int Notify(long nMsg, SmtListenerMsg &param); // 消息通知
```

## 使用说明

### 基本使用

```cpp
// 创建视图
SmtXView* pView = new SmtXView();

// 绑定到窗口
pView->BindWind(hWnd);

// 初始化
pView->InitCreate();
pView->CreateRender();
pView->CreateTools();
```

### 继承使用

```cpp
class CMyMapView : public SmtXView
{
public:
    virtual bool CreateRender(void) override
    {
        // 创建自定义渲染器
        return true;
    }

    virtual bool CreateTools(void) override
    {
        // 创建自定义工具
        return true;
    }
};
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持（消息、监听器等）
- **依赖 MFC**: MFC CView 基类

## 命名空间

模块使用 `Smt_XView` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013
- **作者**: 陈春亮
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. 视图需要正确绑定到窗口后才能使用
2. 渲染器和工具的创建应在 `InitCreate` 之后
3. 视图销毁时应调用 `EndDestory` 进行清理
4. 消息处理需要正确实现 `Notify` 方法

---

# SmtXViewCore

SmartGIS view core module, providing map view base class based on MFC CView.

## Module Overview

SmtXViewCore is the view core module of the SmartGIS system, providing a map view base class `SmtXView` based on MFC CView. This module encapsulates basic map view functionality, including window binding, message handling, render initialization, and tool creation.

## Key Features

### Core Class

#### SmtXView
Inherits from `CView` and `SmtListener`, is the base class for map views.

**Main Features:**
- Window binding: Supports binding to window handles or dialog controls
- Message handling: Handles Windows messages and custom messages
- Render initialization: Creates render devices and renderers
- Tool management: Creates and manages interactive tools
- Menu management: Creates main menu and context menu
- Lifecycle management: View creation and destruction

### Main Interfaces

#### Window Binding
```cpp
long BindWind(HWND hWnd);                    // Bind to window handle
long BindDlgItem(CDialog *pDlg, UINT nItemID); // Bind to dialog control
long UnbindWind(void);                       // Unbind
```

#### Initialization Interfaces
```cpp
virtual bool InitCreate(void);               // Initialize creation
virtual bool EndDestory(void);               // End destruction
virtual bool CreateMainMenu(void);           // Create main menu
virtual bool CreateContexMenu(void);         // Create context menu
virtual bool CreateRender(void);             // Create renderer
virtual bool CreateTools(void);              // Create tools
```

#### Message Handling
```cpp
virtual int Notify(long nMsg, SmtListenerMsg &param); // Message notification
```

## Usage

### Basic Usage

```cpp
// Create view
SmtXView* pView = new SmtXView();

// Bind to window
pView->BindWind(hWnd);

// Initialize
pView->InitCreate();
pView->CreateRender();
pView->CreateTools();
```

### Inheritance Usage

```cpp
class CMyMapView : public SmtXView
{
public:
    virtual bool CreateRender(void) override
    {
        // Create custom renderer
        return true;
    }

    virtual bool CreateTools(void) override
    {
        // Create custom tools
        return true;
    }
};
```

## Dependencies

- **Depends on SmtCore**: Base library support (messages, listeners, etc.)
- **Depends on MFC**: MFC CView base class

## Namespace

The module uses the `Smt_XView` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
- **Author**: 陈春亮
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. View needs to be properly bound to a window before use
2. Renderer and tool creation should be after `InitCreate`
3. View destruction should call `EndDestory` for cleanup
4. Message handling needs proper implementation of `Notify` method

