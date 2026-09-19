<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 遗留层深度抽象（A+B+C）— SP0 伞状契约

**Status:** active  
**Date:** 2026-09-19  
**Scope:** 伞状 living design。锁定 **SP0–SP5** 程序顺序、推荐手法、依赖方向、并行策略与 ABI 边界；**不**开实现计划勾选清单。物理目录拆分（`legacy/render` / `legacy/tool` / `legacy/app` / `legacy/ui`）已完成，本文只约束后续**行为抽象与 Facade 收口**。  
**Related（已接受 / 已落地，勿重开其锁定决策）：**

| 主题 | Spec |
| --- | --- |
| Render leftover 物理迁出 | [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md) |
| App / UI leftover 物理迁出 | [`2026-09-14-app-legacy-split-design.md`](2026-09-14-app-legacy-split-design.md) |
| Tool leftover 物理迁出 | [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md)（landed） |
| RHI + World / GpuScene | [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md) |
| 模型 / 渲染 / 计算伞状 | [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md) |
| Tool session dispatch | [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md) |
| Views ← MFC chrome | [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md) |
| 产品布局 as-built | [`../../build/src-layout.md`](../../build/src-layout.md)、[`../../build/ui-views-skia.md`](../../build/ui-views-skia.md) |

**Child specs（由本伞派生，未写前不得擅自扩大 ABI 或依赖方向）：**

| SP | Spec | Plan |
| --- | --- | --- |
| SP1 tool strangler | [`2026-09-19-legacy-tool-workspace-strangler-design.md`](2026-09-19-legacy-tool-workspace-strangler-design.md) | [`../plans/2026-09-19-legacy-tool-workspace-strangler.md`](../plans/2026-09-19-legacy-tool-workspace-strangler.md) |
| **SP1b** tool 行为搬迁 | [`2026-09-19-tool-behavior-migration-design.md`](2026-09-19-tool-behavior-migration-design.md) | [`../plans/2026-09-19-tool-behavior-migration.md`](../plans/2026-09-19-tool-behavior-migration.md) |
| SP2 present facade | [`2026-09-19-legacy-render-present-facade-design.md`](2026-09-19-legacy-render-present-facade-design.md) | [`../plans/2026-09-19-legacy-render-present-facade.md`](../plans/2026-09-19-legacy-render-present-facade.md) |
| SP3 host behavior | [`2026-09-19-legacy-host-behavior-extract-design.md`](2026-09-19-legacy-host-behavior-extract-design.md) | [`../plans/2026-09-19-legacy-host-behavior-extract.md`](../plans/2026-09-19-legacy-host-behavior-extract.md) |
| SP4 Scene3D → World / GpuScene | [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md) | [`../plans/2026-09-19-scene3d-world-gpuscene.md`](../plans/2026-09-19-scene3d-world-gpuscene.md) |
| SP5 Shell 编译闸门 | [`2026-09-19-shell-compile-gate-design.md`](2026-09-19-shell-compile-gate-design.md) | [`../plans/2026-09-19-shell-compile-gate.md`](../plans/2026-09-19-shell-compile-gate.md) |

---

## 1. 问题与目标

物理拆分已把 2010 leftover 赶到 `src/legacy/**`，终局树（`src/tool`、`src/render/{rhi,scene,skia,math}`、`src/app/views`、`src/content`、`src/ui/views`）边界清晰。但行为仍大量穿过 leftover 类型：`SmtIATool` / `SmtRenderDevice` / MFC `CView` 宿主逻辑与 `scene3d` 引擎相互牵制，终局代码偶发「为了编过而 include legacy」。

本程序（A+B+C）目标：

1. **A — 依赖方向**：终局 **不得** 依赖 `src/legacy/**`；legacy → 终局 Facade（`tool` / `render::rhi` / `content`）允许**单向**。
2. **B — 热点收口**：在工具、Present/Paint、宿主行为三条热路径上用 **Facade strangler** 抽出可测纯逻辑；其余 leftover 保持可编译、可 opt-in。
3. **C — 场景终局对齐**：`legacy/render/scene3d` 的 GIS/三维世界语义逐步迁到 `gis::World` + `render::scene::GpuScene`（见 RHI 双场景），**不**在 leftover 里长第二套引擎。

成功时：新功能写在终局层；leftover 只作适配器与 ABI 壳；`src_all` 不被迫拉 `legacy_*_all`。

---

## 2. 程序表（SP0–SP5，有序）

| ID | 名称 | 职责（锁定） | 前置 | 默认路径所有权（并行时勿重叠） |
| --- | --- | --- | --- | --- |
| **SP0** | 伞状契约 | 本文：顺序、手法、依赖、并行、ABI、子规格成功模板 | — | 仅 `docs/superpowers/specs/` 本文件（+ README Active 行若存在） |
| **SP1** | Tool → Workspace strangler | leftover `SmtIATool` / group tool 经适配器进入 `tool::Workspace`；新交互只写 session dispatch | SP0 | `src/legacy/tool/**`、`src/tool/**`（终局 dispatch 扩展）、相关 `content` 路由薄层 |
| **SP2** | Present / Paint Facade | `legacy/render/bridge`（及必要 GDI/GL 适配）收口到 `render::rhi` present；MFC/`SmtRenderDevice::Init(HWND)` 仍为 leftover 缝 | SP0 | `src/legacy/render/bridge/**`、`src/legacy/render/{gdi,gdi_simple,gl}/**`（适配范围内）、`src/render/rhi/**`（Facade 扩展） |
| **SP3** | Host 行为提取 | `legacy/ui` + `legacy/app` 中与地图会话相关的行为抽到 `content` / `app/views` / `ui::views`；chrome 只走 `content/public` | SP0 | `src/legacy/app/**`、`src/legacy/ui/**`、`src/content/**`、`src/app/views/**`、`src/ui/views/**`（与 Views 迁移规格一致） |
| **SP4** | Scene3D → World / GpuScene | 逻辑世界与 GPU 场景对齐 RHI 双场景；leftover `scene3d`/`model3d`/`terrain` 变薄适配器 | **SP2 缝已落地**（Present Facade 可用） | `src/legacy/render/scene3d/**` 及约定的 `gis/scene`、`render/scene`；**不得**与 SP2 同时改同一 bridge present 热点而无协调 |
| **SP5** | Shell 编译闸门 | 默认产品壳（Views）与 `src_all` 不依赖 leftover；opt-in `legacy_*` 保留；文档与根 README / `src-layout` 与闸门一致 | SP1–SP3 主路径可用；SP4 可并行收尾 | 根 / 文档 / GN group 闸门（`BUILD.gn` 产品组、`docs/build/*`）；**不**借机改 leftover 业务 |

**顺序含义：**

- SP0 先锁定契约；子规格不得与本文矛盾。
- SP1 / SP2 / SP3 **逻辑上同级**（均可在 SP0 后启动），但物理路径必须不重叠。
- SP4 **等待 SP2 的 Present 缝**：否则 Scene 迁移会与 HWND present 适配打架。
- SP5 是程序收口闸门，不是「再开一轮大搬家」。

---

## 3. 推荐手法（锁定）

| 选择 | 说明 |
| --- | --- |
| **主手法** | **Facade strangler**：在 leftover 边界外挂薄 Facade / 适配器，新调用走终局 API；旧 DLL 导出保持。 |
| **提取范围** | **仅在热点**（工具激活与指针事件、Present/Paint、宿主会话组合）提取**纯辅助函数 / 小服务**；禁止借机「全局 traits 化」整棵 leftover。 |
| **Traits** | **推迟**。产品 C++23 traits（`geometry_traits` 等）留给 `algorithm` / `geo` / RHI 终局；不在 SP1–SP3 为 leftover 类型树发明平行 traits 体系。 |
| **增量** | 抽出 → 切换**一个**调用点 → 测 → 再扩。禁止单 PR 整包重写 `scene3d` 或全部 group tools。 |
| **测试** | 新纯逻辑优先 `*_test.cc` 挂终局或 legacy 可选目标；不要求本轮 MFC 全绿作为 SP1–SP3 硬门（与既有 legacy-split 一致）。 |

**明确拒绝的手法：** 整仓 rewrite；为「风格统一」重命名 `Smt_*` 导出；在终局头里 `#include "legacy/…"`；引入 Qt 或第二套控件库。

---

## 4. 依赖规则（锁定）

```
终局 src/{tool,render/{rhi,scene,skia,math},content,app/views,ui/views,sdb,…}
        ▲
        │  禁止依赖
        │
src/legacy/{tool,render,app,ui,…}
        │
        ▼  允许单向
终局 Facade：tool::Workspace / render::rhi / content::ViewHost|MapContents|EventBus 等
```

| 消费者 | 允许依赖 | 禁止 |
| --- | --- | --- |
| `src_all` / 新代码 / Views 壳 | 终局模块与 `content/public` | `src/legacy/**` |
| `legacy/**` | 终局 Facade（rhi / tool dispatch / content public） | 反向把终局实现塞回 legacy 业务核 |
| leftover DLL 互链 | 维持现有部署图；收口时只减不增「终局→legacy」 | 新增 `render` 终局 → `legacy_render` |

与 [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md) 的「`render` 终局 → `legacy_render` **禁止**」一致；本文将其推广到 **全部终局层**。

Chrome（Views）只 include `content/public`（见 tool-event-dispatch / ui-views-mfc-migration）；不得直接链 `t_iatool.h` 或 leftover render 设备头。

---

## 5. 并行策略（锁定）

| 规则 | 内容 |
| --- | --- |
| **可并行** | 多 agent / 多 PR 可同时推进 **SP1、SP2、SP3**，前提是 **路径与 GN 目标不重叠**（见程序表「默认路径所有权」）。 |
| **SP4 门闩** | **SP4 不得在 SP2 Present/Paint Facade 缝可用前**大规模改 `scene3d`→World/GpuScene；可先写 SP4 子规格草稿，实现以 SP2 缝为闸。 |
| **SP5** | 不与 SP1–SP4 抢业务路径；在主路径可演示后改编译闸门与文档。 |
| **冲突处理** | 若必须改共享缝（例如 `content::ToolRouter` 同时服务 SP1 与 SP3），先在子规格写清所有权，再改；禁止 silently 交叉改 sibling 目录。 |
| **Git** | 日常在 **`master`** 上改；不为此程序新开 feature 分支（仓库 agent 规则）。 |

---

## 6. ABI 与部署名（锁定）

| 规则 | 内容 |
| --- | --- |
| **默认** | **不破坏** leftover DLL `dll_stem` / 已文档化的 `Smt_*` 导出与 LoadLibrary 约定（与各 legacy-split 规格一致）。 |
| **例外** | 仅当 **某条子规格显式允许「leftover-only break」**（范围写清：哪些 stem、哪些导出、迁移步骤、opt-in 编译影响）时，才可做 leftover 侧破坏性变更。 |
| **终局** | 新 API 用两层命名空间 + `snake_case` 函数；不要求与 `Smt_*` 同名。 |
| **禁止** | 借深度抽象合并 DLL、改日常部署文件名、或在未授权子规格中「顺便」改 stem。 |

---

## 7. Non-goals

- **禁止 Qt**（及任何第二套桌面控件库替代 Views + Skia）。
- **禁止** leftover 整仓 / 整引擎 wholesale rewrite。
- **不再做** 已完成的物理目录搬家（`legacy/render`、`legacy/tool`、`legacy/app`、`legacy/ui`）；本文不重开「要不要再挪目录」。
- 不把 WinUI / WebView2 / MFC Feature Pack 升为终局壳。
- 不在本伞下复活 D3D9，不 vendor 第二套 GEOS/PROJ/FlyCube。
- 不把 `EventBus` 放进 `src/tool`；不把 FlyCube / Assimp 类型泄漏到公开头（沿用 RHI / model-render-compute）。
- 本伞 **不** 要求删除 leftover 源码；删除或停编以 SP5 + Views parity 为准（见 app-legacy-split / ui-views-mfc-migration）。

---

## 8. 与既有规格的边界

| 既有规格 | 本伞如何对待 |
| --- | --- |
| render / app / tool **legacy-split** | 物理树与「终局不依赖 legacy」已接受；本伞做**下一层行为抽象**，不改已落地路径映射，除非子规格证明 include/GN 漏网。 |
| **render-rhi-scene** | SP2 对齐 Present Facade；SP4 对齐 World / GpuScene。子规格不得另立第二套 RHI。 |
| **tool-event-dispatch** | SP1 的终点是既有 `tool::Workspace` 四通道模型；leftover 经适配器进入，不重开「要不要 EventBus」。 |
| **ui-views-mfc-migration** | SP3 提取宿主行为时遵守「chrome 只组合、地图 HWND + ViewHost」；控件细节仍归 views-controls / leftover-chrome-parity。 |
| **model-render-compute** | 三层语义权威仍在该伞；本伞只管 legacy **如何 strangler 进去**，不复制其 API 表。 |

冲突时：**更窄的已接受实现规格优先**于本伞的表述；本伞负责程序顺序与跨切面规则。若需改已锁定决策，先修订对应 accepted 规格，再改本文交叉引用。

---

## 9. 子规格成功标准模板

每个 SP1–SP5 子规格 **必须** 含以下小节（可增不可减语义）：

1. **Status / Date / Related** — 首行 metadata 含 `Status:`；Related 链回 **本文** 与相关 accepted 规格。  
2. **Goal** — 一条可验证的行为目标（非「清理代码」空话）。  
3. **Non-goals** — 至少写明：不破 ABI（或显式 leftover-only break）、不 Qt、不 wholesale rewrite。  
4. **Locked decisions** — 表格式；含手法（Facade / 提取点）、目标类型或文件列表。  
5. **Path ownership** — 本子规格可改路径；显式列出 **禁改** 的 sibling 路径。  
6. **Dependency** — 重申「终局 ↛ legacy」；列出允许的 legacy → Facade 边。  
7. **ABI** — 默认不变；若 break，专节写范围与迁移。  
8. **Success criteria** — 可勾选、可命令验证（例如 `ninja -C out …`、禁止出现的 include 方向、测试名）。  
9. **Out of scope for later SPs** — 避免吞并 SP4/SP5 工作。

**建议的 Success criteria 骨架（子规格按需填空）：**

1. 终局目标 `…` 在默认 `src_all` / 产品壳路径下 **不再** `#include "legacy/…"`（列出扫描范围）。  
2. leftover 适配器仅通过 `…` Facade 调用终局；单向依赖由 GN `deps` 证明。  
3. `dll_stem` / 导出：`…`（不变或指向 break 专节）。  
4. 测试：`…_test` 覆盖抽出的纯逻辑；热点调用点至少切换 N 处。  
5. 文档：`docs/build/src-layout.md`（及模块 README）与本子规格路径一致。  
6. （SP5 专有）默认 `build.bat app` / `src_all` 不拉 `legacy_*_all`；opt-in 命令仍可用。

---

## 10. 风险与显式不做

| 风险 | 缓解 |
| --- | --- |
| 并行改同一缝导致合并冲突 / 依赖回环 | 路径所有权表 + SP4 等 SP2 |
| 「顺手」traits / 全局重命名烧进度 | 手法锁定：仅热点纯函数 |
| ABI 静默破坏插件 | 默认禁止；break 必须子规格显式授权 |
| SP5 过早关 legacy 导致调试断 | SP5 在 SP1–SP3 主路径可演示后；源码保留 |

---

## 11. Done when（本文）

- [x] Status `active`；版权头 2026 The Mogu Authors  
- [x] SP0–SP5 程序表、手法、依赖、并行、ABI、non-goals 已锁定  
- [x] 交叉链接到 render/app/tool legacy-split、RHI、tool-event-dispatch、ui-views-mfc-migration  
- [x] 子规格成功标准模板齐全  
- [x] 无 TBD / 无与 accepted 规格直接矛盾的条款  

实现工作 **不** 在本文展开；落地以各 SP 子规格 + `docs/superpowers/plans/` 为准。
