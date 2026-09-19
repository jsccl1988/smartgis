<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# WSL mogu sdbd client + DataSourceMgr rename — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Windows `gis/datasource` 经 `SdbdClient` **HTTP+FnRPC 双通道**对接 WSL mogu `sdbd`，`PROVIDER_SDBD` 经 `DataSourceMgr` 打开，活体 e2e 硬依赖 `:8021` 与 `:9032`；同变更将 `SmtDataSourceMgr` 改名为 `DataSourceMgr`。

**Architecture:** 并行两车道——(A) mgr 改名；(B) HTTP `SdbdClient`。汇合后 (C) **FnRPC 传输** + `PROVIDER_SDBD` + `SdbdRemoteDataset` + mgr 接线 + 双通道 live。本地 `SdbdHandler` 保留。

**Tech Stack:** C++23、`net::HttpClient`、`net::RpcClient`、GDAL/OGR、`sdbd_json`、GN `out/`、`build.bat te` / `e2e`、WSL `flow_host --plugin=libsdbd.so`。

**Spec:** [`docs/superpowers/specs/2026-09-19-sdbd-wsl-client-design.md`](../specs/2026-09-19-sdbd-wsl-client-design.md)

## Global Constraints

- Stay on `master`；不新开分支。
- Copyright 2026 Mogu Authors；改动文件 bump year。
- 新/改名函数 `snake_case`；类型 PascalCase；公共命名空间至多两层（`gis` / `gis::datasource`）。
- 注释英文；对话/本计划文档中文。
- 只用 `net::HttpClient`；不改 mogu 仓；不 vendor Qt / 第二 HTTP。
- 产品 API 前缀 `/api/v1/sdbd/*`；本地 Handler 前缀 `/sdbd/api/v1/*` 勿混。
- Live `:8021` **硬失败**（非 SKIP）。
- **不要** `git commit`，除非用户当次明确要求。
- 构建：`build.bat` / `out/` only。
- CBM 查代码优先；并行 agent 改互不重叠路径。

## Parallel lanes

| Lane | Owner paths | Depends |
| --- | --- | --- |
| **A** | `src/gis/datasource/mgr/**` + 全仓 `SmtDataSourceMgr` / `datasourcemgr.h` 调用点 | 无 |
| **B** | 新建 `sdbd_client.*`、mogu 结果类型、client 单测；`BUILD.gn` 仅加新 sources | 无 |
| **C** | `layer.h` `PROVIDER_SDBD`、`ogr_connect`、`sdbd_remote_dataset.*`、mgr `open_dataset` 分支、live 测试、docs | **A+B 完成** |

---

## File map

| Path | Responsibility |
| --- | --- |
| `src/gis/datasource/mgr/datasource_mgr.h/.cc` | 由 `datasourcemgr.*` 改名；类型 `DataSourceMgr` |
| 调用点（legacy/plugin/tests） | include + 类型 + 成员 `snake_case` |
| `src/gis/datasource/gdal/sdbd_client.h/.cc` | mogu HTTP 客户端 |
| `src/gis/datasource/gdal/sdbd_mogu_types.h`（可选并入 client） | mogu JSON 结果 POD |
| `src/gis/datasource/gdal/sdbd_remote_dataset.h/.cc` | 远程产品面 |
| `src/gis/layer/layer.h` | `PROVIDER_SDBD` |
| `ogr_connect.*` | `db_provider_traits<PROVIDER_SDBD>`；`sdbd_base_url(info)` |
| `sde_gdal_test.cc` 或 `sdbd_live_test.cc` | 硬 live e2e |
| `docs/build/src-layout.md`、`docs/README.md` | 短述 |

---

### Task A1: Rename `SmtDataSourceMgr` → `DataSourceMgr`

**Files:**
- Rename: `src/gis/datasource/mgr/datasourcemgr.h` → `datasource_mgr.h`
- Rename: `src/gis/datasource/mgr/datasourcemgr.cpp` → `datasource_mgr.cc`
- Modify: `src/gis/datasource/mgr/BUILD.gn` sources
- Modify: all call sites under `src/` that include `datasourcemgr.h` or name `SmtDataSourceMgr`（见 grep 列表：legacy ui/tool/app、plugin、`sde_gdal_test.cc`、`select_query_test.cc` 等）

**Interfaces:**
- Produces:
  - `class gis::DataSourceMgr`
  - `static DataSourceMgr* get_singleton_ptr();`
  - `static void destroy_instance();`（原 `DestoryInstance` 拼写一并改正）
  - `GDALDataset* open_dataset(const SmtDataSourceInfo& info);`
  - `void close_dataset(GDALDataset*& ds);`
  - 其余成员同步 `snake_case`：`create_mem_vec_layer`、`destroy_mem_vec_layer`、`create_mem_ras_layer`、`destroy_mem_ras_layer`、`open`、`save`、`save_as`、`create_tmp_data_source`、`destroy_tmp_data_source`、`create_data_source`、`delete_data_source`、`get_data_source_count`、`move_first`、`move_next`、`move_last`、`delete_entry`（若与关键字冲突用 `delete_current`）、`is_end`、`get_data_source` 重载、`get_data_source_info`、`get_active_data_source`、`set_active_data_source`
- 头卫：`GIS_DATASOURCE_MGR_DATASOURCE_MGR_H_`
- **不**保留 `typedef SmtDataSourceMgr DataSourceMgr` 或宏别名

- [ ] **Step 1:** 物理改名文件；更新 `BUILD.gn` `sources = [ "datasource_mgr.cc" ]`
- [ ] **Step 2:** 改类名与全部成员为 `snake_case`；修正 `Destory*` → `destroy_*`
- [ ] **Step 3:** 全仓替换 include 路径与符号；编译相关目标
- [ ] **Step 4:** 跑 `out\sde_gdal_test.exe`（或 `build.bat` 聚焦该测试）确认改名未毁本地路径

**Verify:** `rg SmtDataSourceMgr src` 与 `rg datasourcemgr\\.h src` 为零命中（除 changelog/归档外）。

---

### Task B1: `SdbdClient` for mogu `/api/v1/sdbd/*`

**Files:**
- Create: `src/gis/datasource/gdal/sdbd_client.h`
- Create: `src/gis/datasource/gdal/sdbd_client.cc`
- Create: `src/gis/datasource/gdal/sdbd_client_test.cc`（可假 HTTP：先测 URL 拼接 + JSON 解析；活体放 Task C）
- Modify: `src/gis/datasource/gdal/BUILD.gn` — 把 client 加入 `sde_gdal_sources`，`deps += [ "//src/net:net" ]`；加 `test("sdbd_client_test")`

**Interfaces:**
- Consumes: `net::HttpClient::get/post`；现有 `parse_json` / `Json`（`sdbd_json`）
- Produces（`gis::datasource`）:

```cpp
struct MoguHttpResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
};

// Thin mogu client. Default base http://127.0.0.1:8021
class SdbdClient {
 public:
  explicit SdbdClient(std::string base_url = "http://127.0.0.1:8021");
  void set_base_url(std::string base_url);
  const std::string& base_url() const;

  MoguHttpResult get_capabilities(int timeout_sec = 5);
  MoguHttpResult get_collections(int timeout_sec = 5);
  MoguHttpResult get_collection(const std::string& id, int timeout_sec = 5);
  MoguHttpResult get_collection_items(const std::string& id,
                                      const std::string& query = "",
                                      int timeout_sec = 5);
  MoguHttpResult get_coverage(const std::string& id, int timeout_sec = 5);
  MoguHttpResult post_query(const std::string& json_body, int timeout_sec = 30);
  MoguHttpResult post_analyze(const std::string& json_body, int timeout_sec = 60);
  MoguHttpResult post_jobs(const std::string& json_body, int timeout_sec = 30);
  MoguHttpResult get_job(const std::string& id, int timeout_sec = 5);
  MoguHttpResult post_scan(const std::string& json_body, int timeout_sec = 60);
  MoguHttpResult post_tile_window(const std::string& json_body,
                                  int timeout_sec = 60);
  MoguHttpResult post_import(const std::string& json_body, int timeout_sec = 120);
  MoguHttpResult post_import_url(const std::string& json_body,
                                 int timeout_sec = 120);
  MoguHttpResult post_ingest(const std::string& json_body, int timeout_sec = 120);

 private:
  std::string url_for(const std::string& path) const;
  MoguHttpResult get_path(const std::string& path, int timeout_sec);
  MoguHttpResult post_path(const std::string& path, const std::string& body,
                           int timeout_sec);
  std::string base_url_;
  net::HttpClient http_;
};

std::string sdbd_default_base_url();  // env SG_SDBD_BASE or http://127.0.0.1:8021
```

路径必须精确：`/api/v1/sdbd/capabilities` 等（见 spec 路由表）。POST `Content-Type: application/json`。

- [ ] **Step 1:** 写 `sdbd_client_test`：断言 `url_for` / 默认 base；对固定 JSON 用 `parse_json` 抽 `collections` 字段（若 capabilities 样例可内嵌）
- [ ] **Step 2:** 实现 `SdbdClient`；挂入 `BUILD.gn`
- [ ] **Step 3:** `ninja -C out sdbd_client_test`（或 `build.bat` 等价）通过

**Lane A 与 B 可并行。**

---

### Task C1: `PROVIDER_SDBD` + connect traits

**Files:**
- Modify: `src/gis/layer/layer.h` — `eSmtDBProvider` 追加 `PROVIDER_SDBD`
- Modify: `src/gis/datasource/gdal/ogr_connect.h/.cc`

**Interfaces:**
- Produces:

```cpp
template <>
struct db_provider_traits<gis::PROVIDER_SDBD> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = nullptr;  // HTTP, not GDAL driver
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

// Returns base URL: info.szUrl if non-empty, else sdbd_default_base_url().
std::string sdbd_base_url_from_info(const gis::SmtDataSourceInfo& info);
```

`is_db_provider_supported(PROVIDER_SDBD) == true`。  
`make_gdal_open_target` / `make_sdbd_open_target`：对 SDBD **不要**假装成 `SDBD:MEM`；返回空或专用标记由 Task C2 消费（推荐：`open_sdbd_dataset` 不处理 `PROVIDER_SDBD`，由 mgr 分支调用 remote）。

- [ ] **Step 1:** 枚举 + traits + 单测断言 supported / base URL
- [ ] **Step 2:** 编译通过

---

### Task C2: `SdbdRemoteDataset` + `DataSourceMgr::open_dataset` 接线

**Files:**
- Create: `src/gis/datasource/gdal/sdbd_remote_dataset.h/.cc`
- Modify: `datasource_mgr.cc` `open_dataset` / `create_data_source`
- Modify: `BUILD.gn` sources

**Interfaces:**
- Produces:

```cpp
// Product facade over SdbdClient. Not a GDALDataset subclass unless needed;
// prefer returning a GDAL Memory/SDBD wrapper filled from query, OR a
// thin holder that mgr stores as Entry with dataset pointer from
// open_sdbd_dataset after ingest-to-local — prefer: open remote, materialize
// collections into SDBD:MEM layers for OGR consumers (YAGNI: at least one
// collection query → Memory layer round-trip so GetLayer works).
class SdbdRemoteDataset {
 public:
  explicit SdbdRemoteDataset(SdbdClient client);
  bool ping();  // capabilities status 200
  MoguHttpResult capabilities();
  MoguHttpResult list_collections();
  // Materialize: GET collections then items/query into owned GDALDataset*
  // (SDBD:MEM). Caller owns via GDALClose.
  GDALDataset* open_as_gdal_dataset();
 private:
  SdbdClient client_;
};

GDALDataset* open_provider_sdbd_dataset(const gis::SmtDataSourceInfo& info);
```

`DataSourceMgr::open_dataset`：

```cpp
if (info.unProvider == PROVIDER_SDBD) {
  return gis::datasource::open_provider_sdbd_dataset(info);
}
return gis::datasource::open_sdbd_dataset(info);
```

- [ ] **Step 1:** 实现 remote + mgr 分支
- [ ] **Step 2:** 无 live 时至少编译；live 在 C3

---

### Task C3: Live e2e harness（硬依赖）

**Files:**
- Create or extend: `src/gis/datasource/gdal/sdbd_live_test.cc`（推荐独立 exe，挂 `test_all` / `te`）
- Modify: root/`src` test 图、`BUILD.gn`
- Helper: `ensure_sdbd_alive()` in test TU

**Logic for `ensure_sdbd_alive`:**
1. `base = sdbd_default_base_url()`（尊重 `SG_SDBD_BASE`）
2. `SdbdClient(base).get_capabilities()`；`status==200` → ok
3. Else run：

```bat
wsl -e bash -lc "MOGU_ROOT=${SG_MOGU_ROOT:-/home/ccl/dev/src/mogu}; cd \"$MOGU_ROOT\" && (test -x out/sdbd) && (./out/sdbd >/tmp/sdbd_smartgis.log 2>&1 & echo $! > out/sdbd.pid) && sleep 1"
```

（精确启动参数以 mogu `host/main.cpp` / `start.sh` 为准；若需 `flow_host --plugin=libsdbd.so` 则用该命令。）

4. 再 capabilities；仍失败 → `fprintf` + `return 1`（**非 SKIP**）

**Assertions:**
- capabilities 200
- collections 200
- 若有至少一个 collection：items 或 `post_query` 可读
- 无害写：`post_ingest` / `post_import` 仅在 mogu 允许且用唯一临时 id；失败则记录但若 API 501 可标产品限制——**P0 以读面硬绿为准，写面尽力**；若 ingest 可用则断言 2xx
- `DataSourceMgr` + `PROVIDER_SDBD` + `szUrl`/默认 base → `open_dataset` 非空

- [ ] **Step 1:** 实现 harness + 测试
- [ ] **Step 2:** `.\build.bat te` 与 `.\build.bat e2e` 全绿

---

### Task C4: Docs as-built 短述

**Files:**
- Modify: `docs/build/src-layout.md`（datasource / sdbd 段）
- Modify: `docs/README.md`（plan 索引行，若尚未加）

- [ ] **Step 1:** 写明 `DataSourceMgr`、`PROVIDER_SDBD`、双前缀、硬 live
- [ ] **Step 2:** 完成

---

## Plan self-review

| Spec 项 | Task |
| --- | --- |
| SdbdClient `/api/v1/sdbd/*` | B1 |
| PROVIDER_SDBD + mgr 打开 | C1+C2 |
| 硬 live e2e | C3 |
| 本地 Handler 保留 | 不删文件；A/B 不碰 Handler 合同 |
| DataSourceMgr 改名 | A1 |
| net::HttpClient only | B1 |
| 不改 mogu | 全任务 |

无 TBD。并行：A∥B → C。

## Execution

用户已选 **并行落地**：Lane A 与 Lane B 同时开工；汇合后 C1→C4；最后 `build.bat e2e` + `te`。
