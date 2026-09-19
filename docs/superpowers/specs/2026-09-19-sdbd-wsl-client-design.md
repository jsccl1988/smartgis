<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Design: `gis/datasource` 对接 WSL mogu sdbd（HTTP 客户端 + 产品打开路径）

**Status:** accepted  
**Date:** 2026-09-19  
**Scope:** 一套实现计划。Windows SmartGIS 经 **HTTP `:8021` + FnRPC `:9032` 双通道**对接 WSL 侧 mogu `sdbd`，经 `gis::DataSourceMgr` 打开，并纳入硬依赖活体 e2e。  
**Related:** [`2026-09-13-ogr-db-datasource-design.md`](./2026-09-13-ogr-db-datasource-design.md)（GDAL PostGIS/GPKG，非 HTTP）、[`2026-09-13-net-asio-httplib-design.md`](./2026-09-13-net-asio-httplib-design.md)（`net::HttpClient` + `net::RpcClient`）、归档 [`../archive/specs/2026-09-13-mapd-client-design.md`](../archive/specs/2026-09-13-mapd-client-design.md)（曾 defer `SdbdClient`）。

## 已锁定决策

| ID | 决策 |
| --- | --- |
| **SDBD-WSL-SHAPE** | 产品真源 = WSL mogu `sdbd`（`flow_host --plugin=libsdbd.so`）；HTTP 路径前缀 **`/api/v1/sdbd/*`**（mogu 现行），不是本地 `SdbdHandler` 的 `/sdbd/api/v1/*`。 |
| **SDBD-WSL-APPROACH** | 方案 **A**：`SdbdClient` + `PROVIDER_SDBD` + mgr 打开远程；本地 Handler 仅进程内/单测假服务。 |
| **SDBD-WSL-TRANSPORT** | **双通道**：同一 `SdbdClient` 支持 HTTP（`net::HttpClient`）与 FnRPC（`net::RpcClient`，默认口 **9032**，**不是** tabled `:9030`）。`szUrl` / 连接串选传输：`http(s)://host:8021` → HTTP；`sdbd-rpc://host:9032` → FnRPC。环境变量：`SG_SDBD_BASE`（HTTP）、`SG_SDBD_RPC`（如 `127.0.0.1:9032` 或完整 `sdbd-rpc://…`）。e2e **两条都硬测**。 |
| **SDBD-WSL-RPC-KEYS** | FnRPC **不另开合同**：方法名与 HTTP 资源同键，采用 `sdbd.<op>`（如 `sdbd.capabilities`、`sdbd.collections`、`sdbd.query`）；路径参数进 JSON（如 `{"id":"roads"}`）。与 mogu「后置 FnRPC 同键」对齐。 |
| **SDBD-WSL-SURFACE** | 客户端覆盖 mogu 已暴露的 P0 数据面：capabilities、collections（及单集合/items）、query、analyze、jobs、scan、tile_window、import、ingest（与 mogu `register_routes` 对齐；无害写路径在 e2e 中可试）。 |
| **SDBD-WSL-HARD** | `build.bat e2e` / `te` 对 live **HTTP `:8021` 与 FnRPC `:9032` 双硬依赖**：测前可尝试 `infra/sdbd/scripts/run.sh start`；任一不可达则 **失败**（非 SKIP）。 |
| **SDBD-WSL-HTTP** | HTTP 用现有 `net::HttpClient`；RPC 用现有 `net::RpcClient`；不 vendor WinHTTP / 第二栈。 |
| **SDBD-WSL-LOCAL** | 保留 `SdbdHandler` + `SDBD:` GDAL 装饰驱动（Memory/GPKG 等）；**禁止**把本地 Handler 冒充 mogu 真源。 |
| **SDBD-WSL-MGR-NAME** | 原 `SmtDataSourceMgr` **同变更集**改名为 **`gis::DataSourceMgr`**（去掉 `Smt` 前缀，类型 PascalCase）。成员函数按新树规则改为 **`snake_case`**（如 `open_dataset`）。头/源文件改为 `datasource_mgr.h` / `datasource_mgr.cc`（与 `sdbd_*.cc` 一致）。**不**保留 `SmtDataSourceMgr` typedef / 宏别名。更新本仓全部调用点。 |

## Goal

1. 在 `src/gis/datasource` 提供 `gis::datasource::SdbdClient`，对 WSL mogu sdbd 发合同请求。  
2. 增加 `PROVIDER_SDBD`，`DataSourceMgr::open_dataset` 可打开远程空间库，要素进入现有 GDAL / 图层消费路径。  
3. 活体 e2e：探测/拉起 daemon → capabilities → collections/query（及约定写路径）→ mgr 打开，全部必须绿。  
4. 完成 `SmtDataSourceMgr` → `DataSourceMgr` 改名（类型 + 成员 `snake_case` + 文件名 + 调用点）。

## Non-goals

- 不在 mogu 仓增加 `/sdbd/api/v1/*` 兼容 shim（HTTP 前缀仍以 mogu 现行 `/api/v1/sdbd/*` 为准）。  
- **例外（已授权）：** 在 mogu `libsdbd.so` 的 `SdbdProductTraits::pre_run_host` 注册同键 FnRPC `sdbd.*`（`infra/sdbd/http/register_fn_rpc.h`），否则 Windows 双通道硬 e2e 无法绿。  
- 不实现双栈 `legacy|mogu` 客户端开关作为产品默认。  
- 不接 mapd Path R / 瓦片热路径。  
- 不改 `content/public` 合同（除非后续单独任务）。  
- 不借机全局清扫所有遗留 `Smt*` 类型（仅本 mgr 及其本变更触及的符号）。  
- Qt 禁止。

## Architecture

```
DataSourceMgr::open_dataset(info with PROVIDER_SDBD)
        │
        ▼
gis::datasource::SdbdClient(transport from szUrl)
        ├── HTTP:  net::HttpClient  → GET/POST :8021/api/v1/sdbd/*
        └── FnRPC: net::RpcClient   → call("sdbd.<op>", json) :9032
                ▼
WSL mogu flow_host --plugin=libsdbd.so
        │
        ▼
SdbdRemoteDataset (product facade)
        → list collections / query features → OGR / feature path
```

默认 HTTP：`http://127.0.0.1:8021`；默认 RPC：`sdbd-rpc://127.0.0.1:9032`。  
`SG_SDBD_BASE` / `SG_SDBD_RPC` 可覆盖；**未设置时仍连默认**（硬依赖）。

测前启动：`SG_MOGU_ROOT`（默认 `/home/ccl/dev/src/mogu`）下 `bash infra/sdbd/scripts/run.sh start`（优先于遗留 `out/sdbd` 单二进制）。

## Components

| 单元 | 职责 | 依赖 |
| --- | --- | --- |
| `SdbdClient` | mogu `/api/v1/sdbd/*` 方法封装；解析 JSON 为类型化结果 | `//src/net:net` |
| `SdbdRemoteDataset`（名可微调） | `PROVIDER_SDBD` 打开后的产品面：catalog 列表、query→要素、ingest 写回 | Client |
| `db_provider_traits<PROVIDER_SDBD>` + `ogr_connect` | 识别 provider、组装 base URL / open 元数据 | `layer.h` |
| `gis::DataSourceMgr` | `PROVIDER_SDBD` → 远程打开；Memory/GPKG 等仍走现有 `open_sdbd_dataset` | Remote + 现有 SDBD GDAL |
| Live test harness | 探测 capabilities；失败则 `wsl` 拉起 sdbd；再跑合同 + mgr | Client |

公共 C++：两层命名空间（mgr 在 `gis`；客户端在 `gis::datasource`）；内部 `detail`。新/改名函数 `snake_case`。类型 PascalCase。

### 与本地 `SdbdHandler` 的边界

| | 本地 `SdbdHandler` | 远程 `SdbdClient` |
| --- | --- | --- |
| HTTP 前缀 | `/sdbd/api/v1/*`（旧 mgis 风格） | `/api/v1/sdbd/*`（mogu 现行） |
| 角色 | 进程内假服务 / 装饰 GDAL 数据集上的路由 | 产品真源 + 硬 e2e |
| 产品默认 | 否 | 是（`PROVIDER_SDBD`） |

文档与注释必须写清两套前缀，禁止混用为「同一合同」。

### mogu 路由对照（客户端必须覆盖）

与 WSL `infra/sdbd/http/register_routes.cpp` 对齐（实现时以该文件为准）：

| HTTP | Path |
| --- | --- |
| GET | `/api/v1/sdbd/capabilities` |
| GET | `/api/v1/sdbd/collections` |
| GET | `/api/v1/sdbd/collections/:id` |
| GET | `/api/v1/sdbd/collections/:id/items` |
| GET | `/api/v1/sdbd/coverage/:id` |
| POST | `/api/v1/sdbd/query` |
| POST | `/api/v1/sdbd/analyze` |
| POST | `/api/v1/sdbd/jobs` |
| GET | `/api/v1/sdbd/jobs/:id` |
| POST | `/api/v1/sdbd/scan` |
| POST | `/api/v1/sdbd/tile_window` |
| POST | `/api/v1/sdbd/import` |
| POST | `/api/v1/sdbd/import_url` |
| POST | `/api/v1/sdbd/ingest` |

探测健康：优先 `GET .../capabilities`（mogu 无旧版 `/sdbd/api/v1/health` 时不发明第二健康路径）。

## E2E / 硬依赖

1. 测前请求 `GET {base}/api/v1/sdbd/capabilities`。  
2. 失败则尝试：`wsl -e bash -lc 'cd "$MOGU_ROOT" && ./out/sdbd …'`（具体参数跟 mogu host 入口；实现时读 `out/sdbd.log` / `start.sh` 惯例）。  
3. 再失败 → **测试失败**。  
4. 断言：capabilities ok、collections 可列、query（或 items）可读、`DataSourceMgr` + `PROVIDER_SDBD` 打开成功；写路径按 mogu 无害样本（临时 collection / import）覆盖后清理或隔离命名。  
5. 挂入 `sde_gdal_test` 扩展或专用 `sdbd_live_test`，由 `build.bat te` / e2e 测试图执行。

## File map（实现时）

| Path | Responsibility |
| --- | --- |
| `src/gis/layer/layer.h` | `PROVIDER_SDBD` |
| `src/gis/datasource/gdal/sdbd_client.h/.cc` | HTTP 客户端 |
| `src/gis/datasource/gdal/sdbd_remote_dataset.h/.cc` | 远程产品面 |
| `src/gis/datasource/gdal/ogr_connect.h/.cc` | traits / open 元数据 |
| `src/gis/datasource/mgr/datasource_mgr.h/.cc` | 由 `datasourcemgr.*` 改名；类型 `DataSourceMgr`；`PROVIDER_SDBD` 分支 |
| `src/gis/datasource/gdal/BUILD.gn` + mgr `BUILD.gn` + test | deps `//src/net`；live 测试；更新 include |
| 全仓调用点 | `SmtDataSourceMgr` → `DataSourceMgr`；成员 → `snake_case` |
| `docs/build/src-layout.md`（短述） | as-built 指针 |
| 本 spec + 对应 plan | 决策与勾选任务 |

## Testing

- 单元：Client 对 Fake/注入传输或录制 JSON 的解析（若注入缝存在）；否则最小解析单测 + live 为主。  
- Live（硬）：上节 e2e。  
- 回归：现有本地 `SdbdHandler` / `SDBD:MEM` 测试保持绿，证明本地路径未破坏；mgr 改名后编译与调用点全绿。

## Risks

| Risk | Mitigation |
| --- | --- |
| mogu API 与本地 Handler 同名异义 | 文档双前缀；产品只走 mogu |
| WSL 端口未转发 / daemon 未起 | 测前自动拉起；仍失败则硬失败并打印 base URL |
| JSON 形状漂移 | 以 mogu `register_routes` + http unittest 为真源；解析失败明确报错 |
| 写路径污染共享库 | e2e 使用唯一临时 id / 可清理命名 |
| mgr 改名漏改调用点 | 同变更全仓搜 `SmtDataSourceMgr` / 旧头路径；`build.bat te` 硬验收 |

## Spec self-review

- 无 TBD/占位段落；路由表指向 mogu 源文件为真源。  
- 与「硬依赖」和「方案 A」无矛盾；本地 Handler 边界写清。  
- `DataSourceMgr` 命名与新树 PascalCase / 无 `Smt` 前缀一致；成员 `snake_case` 已写入锁定决策。  
- 范围：本仓 datasource + mgr 改名 + 测试；不改 mogu。  
