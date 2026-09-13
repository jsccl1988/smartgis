<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# third_party

对齐 mogu / mgis：`manifest.json` 是 pin 唯一真相；`tools/fetch.py` 拉源码到
**`third_party/.src/`**；`tools/batch.py` 装到 **`third_party/.install`**。
GN 只消费该 prefix（`gn/tp.gni` + `gn/BUILD.gn` 门面），**不再**用
`cmake()` 在 ninja 里编三方。

Gitea 基址与 mogu / mgis 相同：`http://localhost:3000/ccl`（`manifest.json` →
`gitea_mirror`）。`git_url` 一律指向该 org；仓已存在则 **复用** mgis 的 URL +
rev，不重新上传。尚未建仓的包带 `git_url_fallbacks`（GitHub），fetch 会先试
Gitea 再回退。登记见 [GITEA_MIRROR.md](GITEA_MIRROR.md)。

## 入口

```bat
REM 按拓扑安装全部 install_skip=false 包（Debug + MSVC）
build.bat t

REM 单包
build.bat t glog
py -3 third_party\tools\fetch.py --package glog
py -3 third_party\tools\install.py --manifest third_party\manifest.json --package glog --src-root third_party\.src --build-root third_party\.build\glog --install-prefix third_party\.install --stamp third_party\.build\glog\.gn_publish_stamp --unified-prefix --build-type Debug
```

`.src/<name>` 已有完整树（clone / zip / junction）时 fetch **复用**，不会
`rmtree` 进 junction 目标。仅当 `third_party/<name>/CMakeLists.txt` 仍在（整树
vendor）时才 vendored skip。薄 `BUILD.gn` 包装目录 **不** 算源码树。强制重拉：
`set MGIS_TP_FORCE_FETCH=1`。

默认 `build.bat`（产品图）**不**编三方源码。产品只依赖 `//third_party:<name>`。

## 结构

| Path | 说明 |
| --- | --- |
| `manifest.json` | `incubator_third_party_manifest_v1`（GIS 条目复用 mgis；另加本仓包） |
| `tools/` | mogu `fetch.py` / `install.py` / `batch.py` / `deps.py` |
| `.src/` | **所有库源码**（gitignore）：fetch 克隆，或本地 junction → mgis |
| `.build/<pkg>/` | 单包 CMake 树（gitignore） |
| `.install/` | 合并安装前缀（gitignore）；GN `third_party_install_prefix` |
| `gn/` | `tp.gni` + 门面 `config`/`group` |
| `BUILD.gn` | `//third_party:<name>` → `//third_party/gn:<name>` |
| `CxImage/` `antlr4/` `ed25519/` `khronos_gl/` `flycube/` | **薄 GN**（`install_skip`）；源码在 `.src/<name>` |
| `cximage_pub/` | 本仓 MBCS 补丁头（overlay，不上 Gitea） |
| `windows_app_sdk/` | 本地 WinAppSDK / WebView2 解包（不上传；非终局 UI） |
| `python/` | 本地 embeddable CPython（不上传） |
| `bcg/` | **不要**下载或盗版 BCG |
| `GITEA_MIRROR.md` | 各仓 Gitea 状态 |

本机开发：

```bat
mklink /J third_party\.install c:\Dev\src\gis\mgis\out\third_party
mklink /J third_party\.src\gdal c:\Dev\src\gis\mgis\third_party\gdal
```

不要再把 `gdal_sdk` 当公开路径。不要把源码 junction 放在 `third_party/<name>/`
顶层（那会和薄 GN 抢目录）。

## 与 mogu 的差异

- Windows / MSVC：`install.py` 用 VS generator（经 mgis 补丁）
- 不引入 Bazel
- GIS 包清单与 mgis 相同（Gitea URL + rev 原样复用）
- 本仓另有 `khronos_gl` / `flycube` / `antlr4` / `eigen` / `ed25519` 等

`is_build_third_party` 已弃用，不再驱动 `cmake()`。缺库时先 `build.bat t`。

---

**最后更新：** 2026-09-13
