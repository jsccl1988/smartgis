<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GUI / Views 测试方案

本仓桌面壳终局是 **Views + Skia**（[`ui-views-skia.md`](ui-views-skia.md)）。GUI 测试投资跟 `SmartGIS.exe` / `ui::views` 走；leftover MFC（`SmartGIS-Legacy.exe`）只保进程冒烟。

业界对照：单测 → 进程内交互（Chromium [Kombucha](https://chromium.googlesource.com/chromium/src/+/main/chrome/test/interaction/README.md)）→ 壳像素（本仓 L2 为本地 PNG + WIC，非 Skia Gold）→ 黑盒 UIA → 产品冒烟。本仓已落在 **L0 + L1′ + L2 + L4**。

## 分层

| 层 | 名称 | 本仓状态 | 入口 |
| --- | --- | --- | --- |
| **L0** | 工具箱单测（合成事件） | **已有** | `out\views_unittests.exe` |
| **L1** | 进程内交互序列 | **已有**（Wave1；[`ui-testing` P2](#分期) + living §UI interactive harness） | `out\views_interactive_tests.exe`（GN `//src/ui/views:views_interactive_tests`；harness `src/ui/views/testing/harness/`） |
| **L1b** | 壳/合成 perf 微基准 | **已有**（非默认 `te`） | `out\views_bench.exe`（GN `//src/ui/views:views_bench`） |
| **L1c** | UI 视觉取证（Scheme 1 A+C） | **已有**（**非**默认 `te` 门禁） | 失败/`UI_FORENSICS=1` → `out\ui_forensics\`；`tools\debug\scripts\ui_visual_forensics.py` |
| **L1′** | 产品壳语义路径 | **已有** | `SmartGIS.exe --self-test`；`SmartGisWinui.exe --self-test`；`SmartGisCef.exe --self-test`（有 CEF pin 时） |
| **L2** | 壳像素回归 | **已有**；地图帧不进默认基线 | `out\views_pixel_tests.exe` |
| **L3** | 黑盒 UIA / FlaUI | **低优先**（自绘 Views 缺 Provider） | 暂缓 |
| **L4** | 产品 exe 冒烟 | **已有** | `build.bat e2e` → `exe_smoke` |
| **Console L0** | Agent / 命令矩阵 | **规划中**（living §Console coverage） | `content_console_coverage_test` → `build.bat te` |
| **Console L1** | Console / 数据+视口 soft 时序 | **规划中**（非默认 `te`） | `content_console_bench` → `build.bat b` → `console_bench.json` |
| **Console L2** | 壳 Console 驱动冒烟 | **规划中** | `SmartGIS.exe --self-test-console`（+ 可选 OpenCppCoverage） |

GN / 跑法总入口：[`testing/README.md`](../../testing/README.md)。

## 原则

1. **Views 为主、MFC 为辅** — 新用例只加在 `ui::views` / `app/views`；`SmartGIS-Legacy.exe` 仅 `exe_smoke`。
2. **白盒优先于 UIA** — 合成 `MouseEvent` / `KeyEvent`、直接查 View 树与命令状态；不靠屏幕坐标点图。
3. **地图断言走语义** — `MapViewport::wait_ready`、`ViewHost`、`EditSession`、图层/状态栏文案；不断言地图像素。
4. **像素（若做）只测壳** — MenuBar / Tab / StatusBar / 对话框；固定 DIP、关动画；不测 GPU 地图帧。
5. **禁止** — Qt / Squish-for-Qt；不以 WinAppDriver 为主轨；不对 leftover MFC 写大规模 FlaUI。

## 已有能力

### L0 — `views_unittests`

- 路径：`src/ui/views/testing/views_unittests.cc`（GN：`//src/ui/views:views_unittests`）。
- 控制台自测：无 MFC、多数用例不 `CreateWindow`；合成鼠标/键盘驱动 kernel / primitives / GIS 面板。
- 覆盖示例：Theme / Skia canvas API、焦点与 Tab 遍历、BoxLayout、Button / Textfield / Checkbox / Radio / Combobox、TabStrip、Table / AttributeTable、LayerTree、Splitter、ScrollView、**AmboxView**（`CommandCatalog` 分组 + 滚动内容 + `layout_check`）、MenuBar、DPI、`layout_check`。
- 模态挡板：`set_message_box_suppressed_for_test` / `set_file_picker_modals_suppressed_for_test`。
- 布局不变量：`ui/views/kernel/layout_check.h`（`collect_layout_violations`；无 golden 图）。

```bat
build.bat te
out\views_unittests.exe
```

### L1 — `views_interactive_tests`（Wave1 已绿）

- **设计已锁定：** living spec [`2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) §UI interactive harness + overlay bench；实现清单 [`2026-09-28-ui-interactive-overlay-bench.md`](plans/2026-09-28-ui-interactive-overlay-bench.md)。
- **路径：** `src/ui/views/testing/interactive/views_interactive_tests.cc`；harness `src/ui/views/testing/harness/`（`EventGenerator` / `ViewsTestBase` / `OverlayScene`）。
- **GN：** `//src/ui/views:views_interactive_tests` → `out\views_interactive_tests.exe`；行为矩阵 `src/ui/views/testing/interactive/interactive_matrix.md`（仍待补全）。
- **Wave1：** 壳 compositor / `PainterRegistry` / `PaintCommit` overlay；**Wave2：** `MapViewport` / `AuxOverlay` 语义（不读地图像素）。
- **Live 编排：** DebugAgent `ui.*` + `tools/debug/scripts/ui_smoke.py`（与 C++ harness 语义对齐，非替代 L1′）。
- **覆盖率：** OpenCppCoverage 经 `testing/scripts/open_cpp_coverage_views.ps1` 可选 CI；**不**阻塞默认 `build.bat te`。

```bat
build.bat debug views_interactive_tests
out\views_interactive_tests.exe
```

### L1b — `views_bench`（perf，已有；非默认 te）

- **GN：** `//src/ui/views:views_bench` → `out\views_bench.exe`；源码 `src/ui/views/testing/bench/views_bench.cc`。
- **用途：** `EventGenerator.click`、`OverlayScene.commit`（1/4/16 层）、`ShellCompositor.commit+wait` 微基准；CI 可选采集 stdout 时序。
- **Shell perf（U0+）：** DiagnosticTools Trace filter `UI` / category `ui.views` — spans `record_commit` / `present` / compositor `raster`。Debug scenario counters（`PaintCounters`）：`hover_commit_qpc` / `table_scroll_qpc` / `overlay_copy_bytes` / `overlay_commit_qpc`。Bench filters：`--benchmark_filter=BM_hover` / `BM_table` / `BM_overlay`。Soft baseline：machine-local only；no Chromium SLA.

```bat
build.bat debug views_bench
out\views_bench.exe
out\views_bench.exe --benchmark_filter=BM_hover
out\views_bench.exe --benchmark_filter=BM_table
out\views_bench.exe --benchmark_filter=BM_overlay
```

### L1c — UI visual forensics（Scheme 1 A+C；已落地）

- **设计已锁定：** living spec [`2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) §UI visual forensics (A+C)；实现清单 [`2026-09-28-ui-visual-forensics.md`](plans/2026-09-28-ui-visual-forensics.md)。
- **Mode A（默认行为）：** L0 / L1′ 仍以语义断言与 `layout_check` 为门禁；**失败时**（或 `UI_FORENSICS=1`）写入 `out\ui_forensics\<run_id>\`：
  - `layout_issues.txt`（含 sibling-overlap 建议项）
  - `manifest.json`（View 树 bounds；L0 `dump_ui_forensics` 还可写 `frame_0000.png`）
- **C++：** `layout_check` 增补 `collect_sibling_overlaps` / `write_layout_issues_file`；`testing/forensics/ui_forensics.*`；`views_unittests` 场景（TabStrip / Ambox / Gantt geom / dump）；`RenderTracePanel::set_embedded` 收起重复 chrome 以修甘特泳道；DebugAgent `ui.capture_shell` + `:ui capture [path]`。
- **Mode C（可选）：** `tools\debug\scripts\ui_visual_forensics.py` **未**接入默认 `build.bat te`。
  - `--list-runs` / `--analyze <dir>` / `--record-all [--frames N] [--out DIR]`
  - 离线：兄弟 AABB 重叠、TabStrip、Ambox y 间距、Gantt 车道间距（manifest 有度量时）
- **场景矩阵：** Catalog 标签不堆叠；Ambox 无 y 塌缩；Gantt 车道间距；Map↔3D 仅语义 marks，无地图像素 golden。

```bat
dir out\ui_forensics
set UI_FORENSICS=1
out\Debug\views_unittests.exe
py -3 tools\debug\scripts\ui_visual_forensics.py --list-runs
py -3 tools\debug\scripts\ui_visual_forensics.py --analyze out\ui_forensics\<run_id>
```

### L1′ — `SmartGIS.exe --self-test`

- 实现：`src/app/views/main.cc`（`BrowserMain`）。
- 真 HWND：泵消息 → 检查壳 → Map / Data / 3D 切换 → `wait_ready`（`kContentMapView` 时）→ 断言 `HostView::Latest` 出帧（marks：`map-frame-ok` / `scene-frame-ok`）→ 3D trackball 输入 → 编辑点 / 选择 / 清选（`input-point-ok`：`FeatureMutation.geom` 为 point）→ M0 折线 + FeatureGeom（`input-line-ok` / `m0-line-ok`）→ 多边形 digitize（`input-poly-ok` / `input-ok`）→ OGR China PLP 进层（`china-plp-ok`）→ `view.pan`（`pan-ok`）→ 浏览压力（`browse-ok`：多次 LMB pan + wheel，RMB 不被 pan 吞掉；`SKIP_MAP_CONTEXT_MENU=1` 跳过模态菜单）→ 光标处滚轮（`wheel-cursor-ok`）→ 轨道相机矩阵 → `layout_check` → 地图 HWND 与 View bounds 对齐。
- 由 `exe_smoke` 拉起；窗口标题 `SmartGIS Views`。
- 浏览回归 loop：`py -3 testing/tools/loop_runner.py --suite browse`（可 `--no-build`）。
- **Map browse forensic（L1′ 扩展，2026-09-30）：** 卡死 / 花屏黑屏 / 跟手差 / 崩溃 取证。
  - Suites：`browse`（Views 2D）、`browse.3d`（Views 3D tab orbit/wheel）、`legacy.browse.2d`（裸 `SmartGIS-Legacy.exe` Edit + **`os_inject_default=sendinput`** → map_client HWND + `capture_hwnd_bmp_ex` → `legacy-browse-2d-edit.bmp`；`bmp.soft` 时 china score 仅 informational；**zoom_gate** / **motion_gate**（录像唯一帧）/ **click_gate**（click+dblclick + `_click_after.bmp`））、`legacy.browse.3d`（OS inject + scene3d showcase linger / `HARNESS_OS_WAIT_BMP`）。
  - 录像：`HARNESS_RECORD=1`（可选 `HARNESS_RECORD_FPS`、`HARNESS_RECORD_MODE=auto|bmp|ffmpeg`）；`testing/tools/loop/record/hwnd.py`。
    - **双屏：** BMP burst：主屏 HWND 优先 `BitBlt`（跟手）；副屏/遮挡用 `PrintWindow`。`ffmpeg` 优先 `gdigrab title=`；缺 ffmpeg 时 BMP frames 可后编 `mp4_path`（有则写）。报告含 `virtual_screen` / `rect.on_primary` / `ffmpeg_skip` / `mp4_path`。
    - 缺 ffmpeg **不硬失败**；关窗时的全黑尾帧会被丢弃。
  - 产物：`out/<config>/captures/record/<suite>_<stamp>.mp4` 或 `…/record/*_frames/`；分析回放帧在 `captures/analysis/<topic>/`；报告 / marks / showcase BMP 在 `captures/<scenario>/`（与 harness family 对齐：`atmosphere/` `map2d/` `plugin/` `ui/` `legacy/` `shell/`）。报告 JSON 含 `record_path` / `steps[]` / `t_ms`。
  - 症状对照：timeout/`rc=124`→卡死；BMP score / 录像→花屏黑屏；`steps` 时间线 vs 画面→跟手；非零 exit / dump→崩溃（`windbg-crash-diagnose`）。
  - 例：`set HARNESS_RECORD=1` 后 `py -3 testing/tools/loop_runner.py --suite browse.3d --no-build --rounds 1`。双屏强制 BMP：`set HARNESS_RECORD_MODE=bmp`。
  - **As-built note (2026-10-01)：** `legacy.browse.2d` 不再用 `--map2d-showcase` / `HARNESS_OS_WAIT_BMP`；Edit 壳 title `SmartGis`，inject 优先 `AfxFrameOrView*` map client，suite BMP 由 `loop.record.hwnd.capture_hwnd_bmp_ex` 写出。`wheel_burst` **单向**（每 tick 用给定 `delta`，`sendinput`/`postmessage` 一致，不再 `i%2` 翻转）。`suite.json` → `zoom_gate`：首段 zoom-out 前后对 **shell HWND** 落 `legacy/_zoom_before.bmp` / `_zoom_after.bmp`（勿 BitBlt map_client），硬闸 `gates.zoom_pixel_diff`；无变化/after 过黑时升一次 SendInput 并重抓。`.il` 先 zoom-out → pan/path → zoom-in。`browse.3d` / `legacy.browse.3d`（`STEREO_API=OpenGL`）仍走 linger/showcase 路径；`browse`（Views 2D stress）曾会非零退出——正是 forensic 要抓的症状；录像路径已双屏加固。
- 输入/数字化回归 loop：`py -3 testing/tools/loop_runner.py --suite input`（脚本 `harness/shell/input/input.il`；缺省或失败时回退 C++）。
- Suite 契约：`testing/tools/harness/<family>/<suite_id>/` 只留 `suite.json` + 可选 `*.il`（id 与 C++ `ScenarioRegistry` 对齐）；`--list` 列出可用 id。共享 case 在 `harness/_shared/case/`。入口统一 `loop_runner --suite <id>`（已移除同目录 `*_loop.py` / 旁路 `*.args.json`）。
- 算子参数：`run_processing(..., args="{\"k\":\"$var\"}")` 内联（Interact.g4 支持 `\"` / `\\` 转义 + `$var` 展开）。
- UI 交互脚本（A+C）：**Interact DSL** 与 suite 同目录的 `*.il`（grammar `harness/_shared/scripts/grammar/Interact.g4`；ANTLR gen → `out/*/gen`）；经 `content::CapabilityHost` + `shell/runtime`（`capability/` + `interact/`）执行；`ui.*` / `input` / `browse` / `map2d.*` / `atmosphere.*` / `console` / `plugin.*` 均优先跑对应 `.il`（失败回退 C++ body）。DebugAgent：`script.run` / `:script <path>`。详见 living §Harness capability runtime + §UI interact script。
- **IL 操作录制（复现问题，2026-10-01）：** 打开/附着 app → 人工操作 → 生成可回放 `.il`。
  - 入口：`py -3 testing/tools/loop_runner.py --record-il`（或 `loop/record/il_recorder.py`）；`--attach` 附着已开窗口；停录 **Ctrl+Shift+F9** / 控制台 Enter / Ctrl+C。
  - 混合：OS 低级钩子（client 坐标）→ `events.jsonl` → 压缩 `path`/`pan_burst`/`wheel_burst`/`drag`/`click`/`key`；可选 DebugAgent `record.poll` 升成 `@inproc`（如 `select_map_tab`）。无 Agent 时仍产出纯 `@os` `.il`。
  - 产物：`out/<config>/captures/record/il_<stamp>/events.jsonl` + `recorded.il`；可选 `--video` 同录 HWND。
  - 启动默认 `SG_DEBUG=1`；C++：`record.enable` / `record.poll` / `BrowserView::switch_map_tab` 旁路。
- **Visual review closed-loop（2026-10-01；Wave2 同日）：** 按功能出图 → Agent 读 `*.inspect.png` 列 bug → **人工确认** → 闭环修复 → 必要时加严 `score_id`。
  - 入口：`py -3 testing/tools/loop_runner.py --suite <id> --review-prep`（复用已有 BMP；`--force-run` 重跑 showcase）。
  - 产物：`out/<config>/captures/<scenario>/*.inspect.png` + `*_visual_review.json`（`status=pending`；`bugs[]` 由 Agent/人填）。
  - 契约：可选 `suite.json` → `visual_review`（`checklist` / `expect_notes`）；有 `bmp` 的 suite 默认可 review。
  - Wave2 显式 checklist：`legacy.browse.*`、`ui.{shell,catalog,data,scene,interact,interact.os}`、`atmosphere.legacy`、`legacy.map2d/scene3d.*`、`map2d.orthogrid`（外加 Wave1 plugin/atmosphere.full/map2d.china）。Wave2 实跑优先：`legacy.browse.2d`、`map2d.china`。
  - Skill：`.cursor/skills/harness-visual-review/SKILL.md`。**不**进默认 `te`。
  - Living：[`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) §Visual review closed-loop。
  - **Plain argv=[] 2D/3D browse：** `py -3 testing/tools/loop/plain_browse_capture.py`。壳 PrintWindow 用 `views_shell_chrome`（青蓝 map hole 允许）；DXGI 金样优先 FlyCube Present BitBlt + `views_present_dxgi`（拒 TabStrip accent bleed / 壳 ocean clear；可裁顶栏 underline）。Flip/NOREDIRECTION 下 BitBlt 常读不到 swapchain 时，以产品日志为金样（2D：`frame_items=`；3D：`scene3d.present dem` + `lazy attach tab=2`）。3D 用 env `VIEWS_START_MAP_TAB=scene3d`（仍无 argv），不靠 OS 点 TabStrip。
- Console 短路径：`py -3 testing/tools/loop_runner.py --suite console`（`--self-test-console`；`console.il`；marks：`console-ok` / `console-bench-ok`）。
  - 产品合同：`SmartGIS.exe --input-showcase` → `out/Debug/input-self-test-mark.txt`
  - 闸门 marks：`input-point-ok` / `input-line-ok` / `input-poly-ok` / `input-ok`（β `FeatureMutation.geom`）
  - 仅 Map Edit 页（不切 Data/3D），避免完整 `--self-test` 的多页切换开销。
- 完整 `--self-test` 也会写同名 `input-*` marks（在编辑点 / M0 折线 / 多边形段）。
- C# 壳：`SmartGisCs.exe --self-test`（`build.bat cs`）。

| 退出码（节选） | 含义 |
| --- | --- |
| 0 | 通过 |
| 1 | `BrowserView::init` 失败 |
| 2 | 顶层 HWND 无效 |
| 3 / 8 / 10 | Map / Data / Scene `wait_ready` 超时 |
| 4–6 | 内容树 / Catalog 结构异常 |
| 7 / 9 | Data / Scene native HWND 无效 |
| 11–20 | ViewHost / 工具 / 状态栏语义失败 |
| 21–25 | 3D trackball / 输入分发 / 相机未动 |
| 26–29 | OGR 进层失败 / 轨道相机矩阵 / FlyCube present |
| 30–35 | 布局不变量或地图 HWND 几何失败 |
| 36–38 | 图层 / Catalog 空或 HWND 显隐 |
| 39 | China PLP 包络不在中国经纬度范围 |
| 40–42 | `view.pan` 激活或输入分发失败 |
| 49–50 | 浏览压力失败（pan/wheel 崩溃或 RMB 被 pan 吞掉） |
| 60 | M0：`edit.append.linestring` 激活/输入/要素未增加 / FeatureGeom 非 linestring |
| 61 | M0：FeatureInfo 未填充（选择失败或 inspector 空） |
| 62 | M0：`write_path` 失败 |
| 63 | M0：写出 GeoJSON 再打开失败 |
| 64 | Input：`edit.append.polygon` / FeatureGeom 非 polygon |
| 70 | M1：china_city 缺 `text` 注记层 |
| 71 | M1：Style JSON 加载/resolve 失败 |
| 72 | M1：XYZ basemap underlay 零瓦片 |
| 73 | M1：`export_bmp` 失败或非 BM 头 |
| 80 | M2：Processing 面板算子数 &lt; 10 或缺 `native.buffer` / `native.clip` |
| 81 | M2：`native.buffer` 批跑或写回图层失败（&lt;1 feature） |
| 82 | M2：`native.clip` 批跑或写回图层失败（&lt;1 feature） |
| 90 | M3：DEM 种子 / 高程信号失败（`m3-dem-ok`） |
| 91 | M3：3D Tiles 流式 / 缓存失败（`m3-tiles-ok`） |
| 92 | M3：大气开关失败（`m3-atmosphere-ok`） |
| 100 | M4：乐观编辑冲突未检出 |
| 101 | M4：`content::open_map_host_path` 嵌入失败 |

`--self-test` marks（节选）：`m0-*`；`m1-*`；`m2-panel-ok` / `m2-buffer-ok` / `m2-clip-ok`；`m3-dem-ok` / `m3-tiles-ok` / `m3-atmosphere-ok`；`m4-conflict-ok` / `m4-embed-ok`。

### L1′ — Atmosphere 3D showcase（`SmartGIS.exe --atmosphere-showcase=`）

独立于完整 `--self-test`：切到 3D 页，按模式配置大气，连续 `present_gpu` 三帧后退出。
默认 **Null RHI**（确定性 exit 0）。`ATMOSPHERE_SHOWCASE_GPU=1` 时在**独立**
640×480 展示窗上拉 FlyCube/DX12（启动期勿设 `PREFER_FLYCUBE_3D=1`）。
GPU **永远显示直到关掉展示窗**（忽略残留正数 `LINGER_MS`）。自动化用
`ATMOSPHERE_SHOWCASE_TIMED_MS=1500`，或 `LINGER_MS=0` 跳过。
旁路产物：`out/Debug/captures/atmosphere/atmosphere-showcase-mark.txt`、`atmosphere/atmosphere-showcase-<mode>.bmp`、
`atmosphere/atmosphere-showcase-cmdline.txt`。GPU 路径要求 BMP 有可见像素，否则 exit 54。

| 模式 | 含义 |
| --- | --- |
| `land` | 大气未挂载；仅 land/DEM present |
| `ocean` | procedural 场 + ocean on / cloud off |
| `full` | `enable_atmosphere_demo()`（海+云） |
| `coast` | 东海附近 extent + full demo |
| `legacy` | leftover 观感（黑 clear + 海面 + 等高 DEM + 地名）；默认产品脸仍是大气 |

| 退出码 | 含义 |
| --- | --- |
| 0 | 通过 |
| 1 / 2 | init / 顶层 HWND（与自测同） |
| 50 | 3D viewport / present HWND 缺失 |
| 51 | device `create` / `initialize` 失败 |
| 52 | `present_gpu` 失败 |
| 53 | 大气开关或 FieldStore 状态不符 |
| 54 | GPU BMP 全黑 / 单色 clear（无几何信号） |

```bat
set ATMOSPHERE_SHOWCASE_GPU=1
out\SmartGIS.exe --atmosphere-showcase=land
out\SmartGIS.exe --atmosphere-showcase=ocean
out\SmartGIS.exe --atmosphere-showcase=full
out\SmartGIS.exe --atmosphere-showcase=coast
out\SmartGIS.exe --atmosphere-showcase=legacy
py -3 testing\tools\loop_runner.py --suite atmosphere.full --no-build
py -3 testing\tools\loop_runner.py --suite atmosphere.legacy --no-build
```

视觉门禁 suite：`atmosphere.full`（`score_id=atmosphere_full`）；`atmosphere.legacy`（Views Scene3D leftover 观感，`score_id=legacy_scene3d_china`）。

### L1′ — Legacy scene3d showcase（`SmartGIS-Legacy.exe --scene3d-showcase=`）

对照 Views atmosphere / legacy map2d shot loop：无 MDI，`smt_stereo_hwnd_*`
present 三帧后写出旁路 BMP。自动化：`SCENE3D_SHOWCASE_LINGER_MS=0`。

| 模式 | 含义 |
| --- | --- |
| `china` | leftover GL DEM + draped china（默认） |

| 退出码 | 含义 |
| --- | --- |
| 0 | 通过（BMP 旁路已写） |
| 51 | stereo create / resize 失败 |
| 52 | `stereo_hwnd_present` 失败 |
| 54 | BMP 捕获失败 / 无可见像素 |
| 56 / 57 | 旁路路径 / HWND / DLL 失败 |

```bat
set SCENE3D_SHOWCASE_LINGER_MS=0
out\Debug\SmartGIS-Legacy.exe --scene3d-showcase china
py -3 testing\tools\loop_runner.py --suite legacy.scene3d.china --no-build
rem leftover D3D11 stereo:
py -3 testing\tools\loop_runner.py --suite legacy.scene3d.china.d3d --no-build
```

门禁按 leftover GL hypsometric DEM（黑 clear + 陆地绿/棕）：非粉、非贴纸青、
有 landish、不全黑。报告：`out/Debug/captures/legacy/legacy_scene3d_china_loop_report.json`。

### L1′ — UI shell showcase（`SmartGIS.exe --ui-showcase=shell`）

独立壳层截图路径（对照 atmosphere / map2d shot loop）：`Browser::show` → layout_check →
强制子窗 `RedrawWindow` → 写出 `ui-showcase-shell.bmp`。自动化：
`UI_SHOWCASE_LINGER_MS=0`。可选 `UI_FORENSICS=1` 写 `out/ui_forensics/`。

| 退出码 | 含义 |
| --- | --- |
| 0 | 通过（BMP 旁路已写） |
| 2 / 4 | HWND / contents root 缺失 |
| 30 | `layout_check` 硬失败 |
| 54 / 55 | BMP 捕获失败 / 路径失败 |

```bat
set UI_SHOWCASE_LINGER_MS=0
out\Debug\SmartGIS.exe --ui-showcase=shell
py -3 testing\tools\loop_runner.py --suite ui.shell --no-build
```

门禁按默认 **dark** `ThemeService`（shell `#1e1e1e` / accent `#007acc`）：非空白、非品红、
暗色 chrome + 文本/强调色、色桶多样。报告：`out/Debug/captures/ui/ui_shell_loop_report.json`。
map2d china：`py -3 testing/tools/loop_runner.py --suite map2d.china`。

### L1′ — `SmartGisWinui.exe --self-test`

- 实现：`src/app/winui/application.cc`（`OnLaunched`）。
- IDE 区域：MenuBar / Catalog / Ambox / Map|Data|3D / Inspector / StatusBar（与 Views 同语义）。
- 2D（`kMapEdit`）与 3D（`kScene3d`）均要求 `WaitFrameReady` + 非占位 DIB（marks：`map-frame-ok` / `scene-frame-ok`）；Data 页同样 `wait`。
- 窗口标题 `SmartGIS WinUI`；cwd 建议 `out/`。

### L1′ — `SmartGisCs.exe --self-test`

- 实现：`src/app/cs/SmartGisCs/MainWindow.cs`（`RunSelfTestAsync`）。
- 语义与 WinUI C++ **同号同义**；需 `SmartGisRender.exe` 旁路 GPU。
- 窗口标题 `SmartGIS WinUI (C#)`；cwd 建议 `out/`。

### L1′ — `SmartGisCef.exe --self-test`

- 实现：`src/app/cef/self_test.cc`（经 `main.cc` `BrowserMain`）。
- 语义与 Views **同号同义**（上表）；CEF 独有失败只用 **40+**（禁止挪用 Views 语义）：

| 码 | 含义 |
| --- | --- |
| 40 | `CefInitialize` / browser 创建失败 |
| 41 | `web/` 未就绪 / 主 frame 加载失败 |
| 42 | Bridge `Ready` 超时 |
| 43 | Map slot 矩形无效（与 chrome 重叠判定失败或零面积） |

- 需 `smt_build_cef=true` 且 `third_party/cef` Binary Dist pin；缺 pin 时不编 exe（`build.bat cef` 明确失败）。
- 窗口标题 `SmartGIS CEF`。
- 出帧 marks：`map-frame-ok` / `scene-frame-ok`（与 Views / WinUI 同名）。

### L2 — `views_pixel_tests`

- 路径：`src/ui/views/testing/views_pixel_tests.cc`（GN：`//src/ui/views:views_pixel_tests`）；离屏 GDI 捕获 + PNG 基线（WIC 读写）。
- 脚手架：`src/ui/views/testing/pixel_harness.{h,cc}`、`pixel_png_wic.cc`。
- 基线目录：`src/ui/views/testing/testdata/*.png`（壳控件 only：Label+Button、TabStrip、StatusBar、Ambox 默认条；不含 MapViewport 像素）。
- 环境：96 DIP（`device_scale_factor = 1`）、Segoe UI 12px 与 `Theme::measure_text_utf8` 对齐；比较时默认每通道 ±2、坏点比例 ≤ 0.5%。
- 更新基线（仓库根目录 cwd，与 `build.bat te` 一致）：

```bat
out\views_pixel_tests.exe --update-goldens
```

- 已接入 `//:test_all`；`build.bat te` 会编译并运行。

### Console coverage + performance（L0 / L1 / L2）

Living 设计：[`2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) **§Console coverage + performance**；清单 [`2026-09-28-debug-console.md`](plans/2026-09-28-debug-console.md) Task 6。

| 层 | 目标 | 入口 | 产物 / 备注 |
| --- | --- | --- | --- |
| **L0** | `content_console_coverage_test` | `build.bat te`（`//:test_all`） | Headless Agent / 命令矩阵 |
| **L1** | `content_console_bench` | `build.bat b`（`//:benchmark_all`） | `console_bench.json`（数据 + 视口 soft 时序；非默认 `te`） |
| **L2** | `SmartGIS.exe --self-test-console` | 壳 e2e / 自测 | Console 驱动产品路径；JSON 视实现 |

**OpenCppCoverage（可选，不阻塞 `te`）：**

```powershell
powershell -File testing\scripts\open_cpp_coverage_console.ps1
```

- 优先 PATH 上的 `OpenCppCoverage.exe`；缺失则打印 skip 并以 **exit 0** 退出（不炸 CI）。
- 默认跑 `out\Debug\content_console_coverage_test.exe`（可选再跑 `debug_agent_test.exe`）。
- Sources：`src\content\browser\debug`、`src\base\log`。
- 导出 HTML / cobertura → `out\Debug\coverage\console\`。

Views 工具箱可选覆盖率（同样不阻塞 `te`）：`testing\scripts\open_cpp_coverage_views.ps1` → `out\Debug\coverage\views\`。

```bat
build.bat te
build.bat b
out\Debug\SmartGIS.exe --self-test-console
```

### L4 — `exe_smoke`

- 路径：`testing/e2e/exe_smoke.cc`。
- 对各 PE 执行 `--self-test`；缺二进制默认 SKIP，`--require-all` 则 FAIL。

| Binary | `--self-test` 证明 |
| --- | --- |
| `SmartGisRender.exe` | OOP GPU + `FrameReady` + 共享表面 |
| `SmartGIS.exe` | Views 窗 + 地图挂接 / 语义路径 |
| `SmartGisWinui.exe` | WinUI IDE 壳 + Map/Data/3D 出帧（marks：`map-frame-ok` / `scene-frame-ok`） |
| `SmartGisCef.exe` | CEF chrome + 分区 HWND 地图；缺二进制 SKIP |
| `SmartGIS-Legacy.exe` | MFC 主框出现（模态卡住时 harness 关窗） |

```bat
build.bat e2e
```

## 关键路径（应覆盖 / 已覆盖）

| 路径 | 状态 |
| --- | --- |
| 启壳 → Catalog / Ambox / StatusBar 布局 | L1′ + `layout_check` |
| Map \| Data \| 3D 切换 + HWND + `wait_ready` | L1′ |
| 3D trackball 输入不崩 | L1′ |
| `edit.append.point` → undo 可用 → 状态栏 | L1′ |
| `selection.point` / `selection.clear` | L1′ |
| 工具箱控件行为（无真窗） | L0 |
| 壳控件外观（离屏 PNG） | L2 |
| Open 真实工程 → 图层树 → FeatureInfo | **待扩**（假数据夹具） |
| Create Layer / Basemap 对话框 | **待扩**（继续用 modal suppress） |

## 分期

| 阶段 | 内容 | 产出 |
| --- | --- | --- |
| **P0** | `views_unittests` 已入 `//:test_all`；`build.bat te` 会编译并跑 L0；`build.bat e2e` 跑 L1′+L4 | `te` / `e2e` 绿 |
| **P1** | 接入 gtest；按模块拆 `views_unittests`；`--self-test=suite` 可选过滤 | 可过滤套件 |
| **P2** | 轻量交互序列 fixture（进程内 Click → Wait → CheckView + `OverlayScene` 壳 overlay；living spec §UI interactive harness） | `views_interactive_tests`（已绿）+ `interactive_matrix.md`（待补）；Agent `ui.*` + `tools/debug/scripts/ui_smoke.py`（live 壳）；OpenCppCoverage 可选 `testing/scripts/open_cpp_coverage_views.ps1` |
| **P3** | 扩展 L2 场景 + 假数据夹具覆盖 Open 路径 | 外观 + 数据回归 |

## 不做 / 慎做

- 不引入 Qt 测试栈。
- 不以 WinAppDriver / 大规模 FlaUI 作为 Views 主方案。
- 不对 leftover MFC 写深度 UI 自动化。
- 不把地图渲染帧纳入默认 pixel 基线（驱动 / GPU 差异会炸 CI）。
- 不在无 `AutomationId` Provider 时依赖 UIA Name / 坐标点击。

## 相关

| 文档 / 代码 | 角色 |
| --- | --- |
| [`ui-views-skia.md`](ui-views-skia.md) | UI 终局 |
| [`testing/README.md`](../../testing/README.md) | GN 测试入口 |
| [`src/ui/views/README.md`](../../src/ui/views/README.md) | 工具箱 + `views_unittests` + L1 harness |
| [`specs/2026-09-27-views-desktop-shell-design.md`](specs/2026-09-27-views-desktop-shell-design.md) | §UI interactive harness + overlay bench；§UI visual forensics (A+C)；§Console coverage + performance；§Visual review closed-loop |
| [`plans/2026-09-28-ui-interactive-overlay-bench.md`](plans/2026-09-28-ui-interactive-overlay-bench.md) | L1/L1b 实现清单 |
| [`plans/2026-09-28-ui-visual-forensics.md`](plans/2026-09-28-ui-visual-forensics.md) | L1c 取证实现清单 |
| [`plans/2026-10-01-harness-visual-review.md`](plans/2026-10-01-harness-visual-review.md) | Visual review closed-loop 实现清单 |
| [`plans/2026-09-28-debug-console.md`](plans/2026-09-28-debug-console.md) | Debug Console + Task 6 coverage/bench |
| `testing/scripts/open_cpp_coverage_console.ps1` | Console 可选 OpenCppCoverage |
| `testing/scripts/open_cpp_coverage_views.ps1` | Views 可选 OpenCppCoverage |
| [`src/app/views/README.md`](../../src/app/views/README.md) | 产品壳 + `--self-test` |
| `src/ui/views/kernel/layout_check.h` | 布局不变量 |
| `src/ui/views/testing/testdata/` | L2 PNG 基线与说明 |

---

**最后更新：** 2026-10-01
