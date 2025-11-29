# SmtXAMBoxCore

SmartGIS 功能箱核心模块，提供工具和功能的快捷访问界面。

## 模块简介

SmtXAMBoxCore 是 SmartGIS 系统的功能箱核心模块，提供了工具和功能的快捷访问界面。该模块用于在 GIS 应用程序中显示和管理各种工具、功能按钮等，方便用户快速访问常用功能。

## 主要功能

### 功能箱管理
- 工具按钮显示
- 功能分组管理
- 工具图标管理
- 工具状态显示

### 工具管理
- 工具的注册和注销
- 工具的激活和停用
- 工具状态更新
- 工具快捷键支持

### 界面管理
- 功能箱布局
- 按钮样式设置
- 工具提示显示

## 使用说明

### 基本使用

```cpp
// 创建功能箱控件
SmtXAMBox* pAMBox = new SmtXAMBox();

// 初始化
pAMBox->Init();

// 注册工具
pAMBox->RegisterTool(pTool);

// 激活工具
pAMBox->ActivateTool(toolId);
```

### 工具注册

```cpp
// 注册工具
SmtToolInfo toolInfo;
toolInfo.id = TOOL_ID_ZOOM;
toolInfo.name = "缩放";
toolInfo.icon = "zoom.ico";
pAMBox->RegisterTool(toolInfo);
```

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 SmtToolCore**: 工具核心
- **依赖 MFC**: MFC 控件支持

## 命名空间

模块使用 `Smt_XAMBox` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013

## 注意事项

1. 功能箱需要正确初始化后才能使用
2. 工具注册应在系统启动时完成
3. 工具状态变化需要及时更新界面

---

# SmtXAMBoxCore

SmartGIS function box core module, providing quick access interface for tools and functions.

## Module Overview

SmtXAMBoxCore is the function box core module of the SmartGIS system, providing a quick access interface for tools and functions. This module is used to display and manage various tools and function buttons in GIS applications, facilitating quick access to commonly used functions.

## Key Features

### Function Box Management
- Tool button display
- Function grouping management
- Tool icon management
- Tool status display

### Tool Management
- Tool registration and unregistration
- Tool activation and deactivation
- Tool status updates
- Tool shortcut key support

### Interface Management
- Function box layout
- Button style settings
- Tooltip display

## Usage

### Basic Usage

```cpp
// Create function box control
SmtXAMBox* pAMBox = new SmtXAMBox();

// Initialize
pAMBox->Init();

// Register tool
pAMBox->RegisterTool(pTool);

// Activate tool
pAMBox->ActivateTool(toolId);
```

### Tool Registration

```cpp
// Register tool
SmtToolInfo toolInfo;
toolInfo.id = TOOL_ID_ZOOM;
toolInfo.name = "Zoom";
toolInfo.icon = "zoom.ico";
pAMBox->RegisterTool(toolInfo);
```

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on SmtToolCore**: Tool core
- **Depends on MFC**: MFC control support

## Namespace

The module uses the `Smt_XAMBox` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013

## Notes

1. Function box needs proper initialization before use
2. Tool registration should be completed at system startup
3. Tool status changes need to update interface in time

