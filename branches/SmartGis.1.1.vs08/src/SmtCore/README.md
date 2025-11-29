# SmtCore

SmartGIS 系统核心基础库，提供系统运行所需的基础功能和服务。

## 模块简介

SmtCore 是 SmartGIS 系统的核心基础库，提供了系统运行所需的各种基础功能，包括线程管理、内存管理、日志系统、插件管理、消息机制、文件操作等核心服务。

## 主要功能

### 1. 线程管理
- **smt_thread.h/cpp**: 线程封装类
- **smt_threadpool.h/cpp**: 线程池管理
- **smt_workthread.cpp**: 工作线程实现
- **smt_trd_sync.h**: 线程同步相关定义

### 2. 内存管理
- **smt_mempool.h/cpp**: 内存池实现
- **smt_mempoolmgr.h/cpp**: 内存池管理器
- **smt_memshare.h/cpp**: 共享内存管理

### 3. 日志系统
- **smt_log.h/cpp**: 日志记录功能
- **smt_logmanager.h/cpp**: 日志管理器

### 4. 插件系统
- **smt_plugin.h/cpp**: 插件接口和实现
- **smt_pluginmanager.h/cpp**: 插件管理器

### 5. 消息机制
- **smt_msg.h/cpp**: 消息定义和处理
- **smt_msg_def.h**: 消息定义
- **smt_listener.h/cpp**: 监听器接口
- **smt_listenermanager.h/cpp**: 监听器管理器

### 6. 文件系统
- **smt_filesys.h/cpp**: 文件系统操作封装

### 7. 动态库管理
- **smt_dynlib.h/cpp**: 动态库加载和调用
- **smt_dynlibmanager.h/cpp**: 动态库管理器

### 8. XML 处理
- **smt_xml.h/cpp**: XML 解析和处理
- **smt_xmlparser.cpp**: XML 解析器
- **smt_xmlerror.cpp**: XML 错误处理

### 9. 同步机制
- **smt_cslock.h/cpp**: 临界区锁
- **smt_srwlock.h/cpp**: 读写锁

### 10. 其他功能
- **smt_timer.h/cpp**: 定时器功能
- **smt_command.h/cpp**: 命令模式实现
- **smt_winservice.h/cpp**: Windows 服务支持
- **smt_exception.h**: 异常处理
- **smt_assert.h/cpp**: 断言功能
- **smt_matrix2d.h**: 2D 矩阵定义

## 核心 API

### 基础 API (smt_api.h)
- 变量转换函数（VarToBool, VarToByte, VarToInteger 等）
- 字符串处理函数
- 数值比较函数
- 文件路径处理函数

### 错误码定义
系统定义了完整的错误码枚举（SmtErr），包括：
- `SMT_ERR_NONE`: 正确
- `SMT_ERR_FAILURE`: 失败
- `SMT_ERR_NOT_ENOUGH_MEM`: 内存不足
- `SMT_ERR_INVALID_HANDLE`: 无效句柄
- `SMT_ERR_INVALID_PARAM`: 无效参数
- 等等

## 数据结构

### 基础结构 (smt_bas_struct.h)
定义了系统使用的基础数据结构。

### 环境结构 (smt_env_struct.h)
定义了系统环境相关的数据结构。

## 使用说明

### 初始化
在使用其他模块之前，需要先初始化 SmtCore 模块。

### 线程管理
```cpp
// 创建线程
SmtThread* pThread = new SmtThread();
pThread->Start();

// 使用线程池
SmtThreadPool* pPool = SmtThreadPool::GetInstance();
pPool->AddTask(task);
```

### 日志记录
```cpp
SmtLogManager* pLogMgr = SmtLogManager::GetInstance();
pLogMgr->Log(LOG_INFO, "Message");
```

### 插件管理
```cpp
SmtPluginManager* pPluginMgr = SmtPluginManager::GetInstance();
pPluginMgr->LoadPlugin("plugin.dll");
```

## 依赖关系

SmtCore 是系统的基础模块，其他模块通常都依赖于它。

## 版本信息

- **版本**: 1.0
- **作者**: 陈春亮
- **日期**: 2010.11.17
- **版权**: Copyright (c) 2010 CCL. All rights reserved.

## 注意事项

1. 本模块是系统的基础库，其他模块都依赖于此模块
2. 线程和内存管理需要正确初始化和释放
3. 日志系统建议在系统启动时初始化
4. 插件系统支持动态加载和卸载插件

---

# SmtCore

SmartGIS system core base library, providing fundamental functions and services required for system operation.

## Module Overview

SmtCore is the core base library of the SmartGIS system, providing various fundamental functions required for system operation, including thread management, memory management, logging system, plugin management, message mechanism, file operations, and other core services.

## Key Features

### 1. Thread Management
- **smt_thread.h/cpp**: Thread wrapper class
- **smt_threadpool.h/cpp**: Thread pool management
- **smt_workthread.cpp**: Work thread implementation
- **smt_trd_sync.h**: Thread synchronization definitions

### 2. Memory Management
- **smt_mempool.h/cpp**: Memory pool implementation
- **smt_mempoolmgr.h/cpp**: Memory pool manager
- **smt_memshare.h/cpp**: Shared memory management

### 3. Logging System
- **smt_log.h/cpp**: Logging functionality
- **smt_logmanager.h/cpp**: Log manager

### 4. Plugin System
- **smt_plugin.h/cpp**: Plugin interface and implementation
- **smt_pluginmanager.h/cpp**: Plugin manager

### 5. Message Mechanism
- **smt_msg.h/cpp**: Message definition and handling
- **smt_msg_def.h**: Message definitions
- **smt_listener.h/cpp**: Listener interface
- **smt_listenermanager.h/cpp**: Listener manager

### 6. File System
- **smt_filesys.h/cpp**: File system operation wrapper

### 7. Dynamic Library Management
- **smt_dynlib.h/cpp**: Dynamic library loading and calling
- **smt_dynlibmanager.h/cpp**: Dynamic library manager

### 8. XML Processing
- **smt_xml.h/cpp**: XML parsing and processing
- **smt_xmlparser.cpp**: XML parser
- **smt_xmlerror.cpp**: XML error handling

### 9. Synchronization
- **smt_cslock.h/cpp**: Critical section lock
- **smt_srwlock.h/cpp**: Read-write lock

### 10. Other Features
- **smt_timer.h/cpp**: Timer functionality
- **smt_command.h/cpp**: Command pattern implementation
- **smt_winservice.h/cpp**: Windows service support
- **smt_exception.h**: Exception handling
- **smt_assert.h/cpp**: Assertion functionality
- **smt_matrix2d.h**: 2D matrix definition

## Core API

### Base API (smt_api.h)
- Variable conversion functions (VarToBool, VarToByte, VarToInteger, etc.)
- String processing functions
- Numeric comparison functions
- File path processing functions

### Error Code Definitions
The system defines a complete error code enumeration (SmtErr), including:
- `SMT_ERR_NONE`: Success
- `SMT_ERR_FAILURE`: Failure
- `SMT_ERR_NOT_ENOUGH_MEM`: Insufficient memory
- `SMT_ERR_INVALID_HANDLE`: Invalid handle
- `SMT_ERR_INVALID_PARAM`: Invalid parameter
- etc.

## Data Structures

### Base Structures (smt_bas_struct.h)
Defines base data structures used by the system.

### Environment Structures (smt_env_struct.h)
Defines data structures related to system environment.

## Usage

### Initialization
Initialize the SmtCore module before using other modules.

### Thread Management
```cpp
// Create thread
SmtThread* pThread = new SmtThread();
pThread->Start();

// Use thread pool
SmtThreadPool* pPool = SmtThreadPool::GetInstance();
pPool->AddTask(task);
```

### Logging
```cpp
SmtLogManager* pLogMgr = SmtLogManager::GetInstance();
pLogMgr->Log(LOG_INFO, "Message");
```

### Plugin Management
```cpp
SmtPluginManager* pPluginMgr = SmtPluginManager::GetInstance();
pPluginMgr->LoadPlugin("plugin.dll");
```

## Dependencies

SmtCore is the base module of the system, and other modules typically depend on it.

## Version Information

- **Version**: 1.0
- **Author**: 陈春亮
- **Date**: 2010.11.17
- **Copyright**: Copyright (c) 2010 CCL. All rights reserved.

## Notes

1. This module is the base library of the system, and other modules depend on it
2. Thread and memory management need proper initialization and cleanup
3. Logging system should be initialized at system startup
4. Plugin system supports dynamic loading and unloading of plugins

