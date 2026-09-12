# SmtToolCore

SmartGIS 工具核心模块，提供交互工具的基础框架和管理功能。

## 模块简介

SmtToolCore 是 SmartGIS 系统的工具核心模块，提供了交互工具的基础框架和管理功能。该模块定义了工具接口、工具管理器等，支持各种地图交互工具（如缩放、平移、选择、编辑等）的注册和管理。

## 主要功能

### 工具接口 (SmtIATool)
- 工具基类定义
- 工具注册和注销
- 工具激活和停用
- 工具消息处理
- 工具委托机制

### 工具管理器 (SmtIAToolManager)
- 工具注册管理
- 工具激活管理
- 工具状态管理
- 工具消息分发

### 工具消息
- 工具消息定义
- 工具消息传递
- 工具事件处理

## 核心接口

### SmtIATool

```cpp
// 注册和注销
virtual int Register(void);
virtual int RegisterMsg(void);
virtual int UnRegister(void);
virtual int UnRegisterMsg(void);

// 激活
virtual int SetActive();

// 初始化
virtual int Init(HWND hWnd, pfnToolCallBack pfnCallBack = NULL, void* pToFollow = NULL);

// 绘制和定时器
virtual int AuxDraw(void);
virtual int Timer(void);

// 结束交互
virtual int EndIA(long nMsg, SmtListenerMsg &param);

// 委托机制
virtual bool BeginDelegate(SmtIATool * pDelegateTag);
virtual int EndDelegate(bool bReleaseTarTool = true);
```

## 使用说明

### 创建自定义工具

```cpp
class CMyTool : public SmtIATool
{
public:
    CMyTool() : SmtIATool() {}
    virtual ~CMyTool() {}

    // 注册工具
    virtual int Register(void) override
    {
        return SmtIATool::Register();
    }

    // 激活工具
    virtual int SetActive() override
    {
        // 设置工具状态
        return SmtIATool::SetActive();
    }

    // 处理鼠标事件
    virtual int OnMouseDown(UINT nFlags, CPoint point) override
    {
        // 处理鼠标按下
        return SMT_OK;
    }
};
```

### 使用工具管理器

```cpp
// 获取工具管理器
SmtIAToolManager* pToolMgr = SmtIAToolManager::GetInstance();

// 注册工具
CMyTool* pTool = new CMyTool();
pToolMgr->RegisterTool(pTool);

// 激活工具
pToolMgr->SetActiveTool(TOOL_ID_MY_TOOL);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持（消息、监听器等）
- **依赖 SmtRender**: 渲染支持

## 命名空间

模块使用 `Smt_IATool` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013
- **作者**: 陈春亮
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. 工具需要正确注册后才能使用
2. 工具激活前需要初始化
3. 工具消息需要正确注册和处理
4. 工具委托机制可以用于工具组合

---

# SmtToolCore

SmartGIS tool core module, providing interactive tool base framework and management functionality.

## Module Overview

SmtToolCore is the tool core module of the SmartGIS system, providing the base framework and management functionality for interactive tools. This module defines tool interfaces, tool managers, etc., supporting registration and management of various map interactive tools (such as zoom, pan, select, edit, etc.).

## Key Features

### Tool Interface (SmtIATool)
- Tool base class definition
- Tool registration and unregistration
- Tool activation and deactivation
- Tool message handling
- Tool delegation mechanism

### Tool Manager (SmtIAToolManager)
- Tool registration management
- Tool activation management
- Tool status management
- Tool message distribution

### Tool Messages
- Tool message definitions
- Tool message passing
- Tool event handling

## Core Interfaces

### SmtIATool

```cpp
// Registration and unregistration
virtual int Register(void);
virtual int RegisterMsg(void);
virtual int UnRegister(void);
virtual int UnRegisterMsg(void);

// Activation
virtual int SetActive();

// Initialization
virtual int Init(HWND hWnd, pfnToolCallBack pfnCallBack = NULL, void* pToFollow = NULL);

// Drawing and timer
virtual int AuxDraw(void);
virtual int Timer(void);

// End interaction
virtual int EndIA(long nMsg, SmtListenerMsg &param);

// Delegation mechanism
virtual bool BeginDelegate(SmtIATool * pDelegateTag);
virtual int EndDelegate(bool bReleaseTarTool = true);
```

## Usage

### Creating Custom Tools

```cpp
class CMyTool : public SmtIATool
{
public:
    CMyTool() : SmtIATool() {}
    virtual ~CMyTool() {}

    // Register tool
    virtual int Register(void) override
    {
        return SmtIATool::Register();
    }

    // Activate tool
    virtual int SetActive() override
    {
        // Set tool status
        return SmtIATool::SetActive();
    }

    // Handle mouse events
    virtual int OnMouseDown(UINT nFlags, CPoint point) override
    {
        // Handle mouse down
        return SMT_OK;
    }
};
```

### Using Tool Manager

```cpp
// Get tool manager
SmtIAToolManager* pToolMgr = SmtIAToolManager::GetInstance();

// Register tool
CMyTool* pTool = new CMyTool();
pToolMgr->RegisterTool(pTool);

// Activate tool
pToolMgr->SetActiveTool(TOOL_ID_MY_TOOL);
```

## Dependencies

- **Depends on SmtCore**: Base library support (messages, listeners, etc.)
- **Depends on SmtRender**: Rendering support

## Namespace

The module uses the `Smt_IATool` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
- **Author**: 陈春亮
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. Tools need proper registration before use
2. Tools need initialization before activation
3. Tool messages need proper registration and handling
4. Tool delegation mechanism can be used for tool composition

