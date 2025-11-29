# SmtMapServer

SmartGIS 地图服务器，提供 WebGIS 地图服务功能。

## 模块简介

SmtMapServer 是 SmartGIS 系统的地图服务器模块，提供了 WebGIS 地图服务功能。该模块支持基于线程池的多线程并发地图服务，实现了 OGC 标准地图服务访问接口（WMS、WTS）。

## 主要功能

### 地图服务
- WMS（Web Map Service）服务
- WTS（Web Tile Service）服务
- 地图瓦片生成
- 地图服务管理

### 服务器功能
- 多线程并发处理
- 线程池管理
- 请求处理
- 响应生成

## 依赖关系

- **依赖 SmtCore**: 基础库支持
- **依赖 SmtGisCore**: GIS 数据模型
- **依赖 SmtRender**: 渲染支持

## 命名空间

模块使用 `Smt_MapServer` 命名空间。

## 版本信息

- **版本**: 1.0
- **开发时间**: 2010-2013

---

# SmtMapServer

SmartGIS map server, providing WebGIS map service functionality.

## Module Overview

SmtMapServer is the map server module of the SmartGIS system, providing WebGIS map service functionality. This module supports multi-threaded concurrent map services based on thread pools, implementing OGC standard map service access interfaces (WMS, WTS).

## Key Features

### Map Services
- WMS (Web Map Service) service
- WTS (Web Tile Service) service
- Map tile generation
- Map service management

### Server Features
- Multi-threaded concurrent processing
- Thread pool management
- Request handling
- Response generation

## Dependencies

- **Depends on SmtCore**: Base library support
- **Depends on SmtGisCore**: GIS data model
- **Depends on SmtRender**: Rendering support

## Namespace

The module uses the `Smt_MapServer` namespace.

## Version Information

- **Version**: 1.0
- **Development Period**: 2010-2013
