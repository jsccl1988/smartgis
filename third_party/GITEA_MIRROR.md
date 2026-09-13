<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis third_party Gitea mirror

与 mogu / mgis 同一套本地 Gitea。**不另造包系统。** Pins 在
[`manifest.json`](manifest.json)（`incubator_third_party_manifest_v1`）。

源码落地目录一律是 **`third_party/.src/<name>`**（gitignore）。仓库里只保留薄
GN / overlay。`git_url` 指向 `http://localhost:3000/ccl/<lib>.git`。

已在 Gitea 上的仓 **直接复用** mgis `third_party/manifest.json` 的 URL + rev，
不重新上传。尚未建仓的条目带 `git_url_fallbacks`（GitHub）；fetch 先试 Gitea，
失败再回退。不要把 GitHub 写进 `git_url` 充作已镜像。

## Repos

| Library | Status | URL | git_ref | Notes |
| --- | --- | --- | --- | --- |
| glog | reused | http://localhost:3000/ccl/glog | `v0.6.0` | mogu pin |
| googletest | reused | http://localhost:3000/ccl/googletest | `v1.14.0` | mogu pin |
| benchmark | reused | http://localhost:3000/ccl/googlebenchmark | `cb7267c1…` | mgis import；`.src/benchmark` |
| asio | reused | http://localhost:3000/ccl/asio | `asio-1-30-2` | Gitea 已有仓；GitHub fallback |
| cpp-httplib | reused | http://localhost:3000/ccl/cpp-httplib | `v0.18.3` | mogu pin |
| sqlite3 | reused | http://localhost:3000/ccl/sqlite3 | `527d3eb0…` | mgis import |
| geos | reused | http://localhost:3000/ccl/geos | `4fc2ba18…` | mgis import |
| PROJ | reused | http://localhost:3000/ccl/PROJ | `1c7cf0dc…` | mgis import |
| SQLiteCpp | reused | http://localhost:3000/ccl/SQLiteCpp | `85a256f6…` | mgis import |
| libspatialindex | reused | http://localhost:3000/ccl/libspatialindex | `a7004ccc…` | mgis import |
| gdal | reused | http://localhost:3000/ccl/gdal | `8bc09143…` | mgis import |
| libspatialite | reused | http://localhost:3000/ccl/libspatialite | `1c0e08e1…` | mgis import |
| SpatiaLiteCpp | reused | http://localhost:3000/ccl/SpatiaLiteCpp | `ec2b1d83…` | mgis import |
| CxImage | reused | http://localhost:3000/ccl/CxImage | `f36a4a5b…` | mgis import；薄 GN 编 `.src/CxImage` |
| wtl8.0 | reused | http://localhost:3000/ccl/wtl8.0 | `8ab4baa8…` | mgis import；头文件在 `.src/wtl8.0` |
| eigen | reused | http://localhost:3000/ccl/eigen | `3.4.0` | header-only；orthogrid Laplace SparseLU |
| antlr4 | Gitea empty; official zip | https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip | `4.13.2` | C++ runtime only（BSD-3）。`ccl/antlr4` 空仓，未改用 git_url |
| flycube | **uploaded** | http://localhost:3000/ccl/flycube | `1b033121…` | 浅克隆 unshallow 后推 `master`；GitHub fallback 仍保留 |
| assimp | **Gitea 404** | http://localhost:3000/ccl/assimp | `v5.4.3` | `C:\Dev\src\open` 无本地树。`smt_has_assimp` 在 `.src/assimp/include/assimp/Importer.hpp` 出现前为 false |
| tinygltf | **on disk via tag archive** | http://localhost:3000/ccl/tinygltf | `v2.9.3` | `.src/tinygltf` 来自 GitHub `v2.9.3` tar.gz（Gitea 空仓） |
| khronos_gl | **uploaded** | http://localhost:3000/ccl/khronos_gl | `e40d74db…` | 从 `.src/khronos_gl_upload` 推 `main` |
| ed25519 | **uploaded** | http://localhost:3000/ccl/ed25519 | `51854966…` | tweetnacl 合订；`.src/ed25519` 推 `main` |

未上传（有意）：`.install` 合并前缀（本地可 junction → mgis `out/third_party`）、
`windows_app_sdk` / WebView2 二进制、`cximage_pub`（本仓 overlay）、`python`
embeddable zip、`bcg`（不要盗版）、Chromium 稀疏树（太大，不镜像到本 Gitea）。

## Pull

```bat
build.bat t
py -3 third_party\tools\fetch.py --all
```

本地已有 mgis 源码时，可把 `.src/<name>` junction 过去，fetch 会 reuse：

```bat
mklink /J third_party\.src\CxImage c:\Dev\src\gis\mgis\third_party\CxImage
```

## Token（与 WSL incubator 相同）

`gitea_token()` 顺序（`incubator/src/third_party/tools/mirror_urls.py`）：

1. 环境变量 `GITEA_TOKEN` 或 `INCUBATOR_GITEA_TOKEN`
2. 文件：`src/third_party/.gitea_token`、仓库根 `.gitea_token`、`~/.config/incubator/gitea_token`（可用 `INCUBATOR_GITEA_TOKEN_FILE` 覆盖）
3. 本地 Docker：`docker exec -u git git-server-gitea gitea admin user generate-access-token -u ccl -t incubator-mirror --raw --scopes all`

本机已用 WSL 第 2 步的 incubator `.gitea_token` 完成下面三次 push。**不要把 token 写进本仓。** Git HTTP 用 `http://oauth2:<token>@localhost:3000/ccl/<lib>.git`，不要把带 token 的 remote 留在 `.git/config`。

## 已上传（2026-09-13）

| 仓 | 分支 | SHA |
| --- | --- | --- |
| `ccl/khronos_gl` | `main` | `e40d74db4e03df9ee05667f787c51e68c0c2e0c6` |
| `ccl/ed25519` | `main` | `51854966deb651a149b6cd3d95cf7d9bd33acdf9` |
| `ccl/flycube` | `master` | `1b03312157e97286e5737c24695a5e5313138c52` |

GIS 包不要重新导入。`assimp` / `tinygltf` 仍缺本地 `.src` 树，未推。

---

**最后更新：** 2026-09-13
