<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# glang 独占 CPython，插件只登记 — 实施清单

> 规格：[`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) **§glang 独占解释器与 glang.plugin 能力表**。图：[`../diagrams/plugin-python-runtime.html`](../diagrams/plugin-python-runtime.html)。

**目标：** 进程内唯一 CPython 3.12 归 `app::py::Interpreter`。能力按名进 `glang.plugin`。套件脚本改为真 Python 的 `main()`。opcode 管线与 `smartgis` 扩展面在转绿后删除。

**架构：** 插件运行时只做清单、`start` / `stop`、登记 / 注销。`eval_*` 留在 C++。`content/public` 的 `PluginHost` 签名不动。Debug 保持桩（`python_enabled` 在 `is_debug` 时为 false）。

本文件是拆解，供阅读和按段验收。每一段的完成标准只有该段的「门禁」：命令跑过，并且写出的断言成立。后一段不得跳过前一段的门禁。不要把「每步提交」当成现在就要改 C++ 的指令。

## 全局约束

- 留在 `master`。不新开分支。
- 不改 `content/public/` 的 `PluginHost` 签名。
- 不第二解释器，不 Qt / PyQt。
- 内核（TIN、GEOS、场景写入）留在 C++。进内核和截图前释放 GIL。
- 公共 C++ 停在两层：登记 API 在 `app::py`。Python 模块名是 `glang.plugin`。
- 不增长 `Op`（`Op::kCount` 保持 105，直到阶段 5 删除整张表）。
- 套件门禁不放宽：`required_marks` 全部落地；有 BMP 的 suite 还要 `score_id` 为 ok。`visual_review` 不参与失败。`plugin.report` 无 BMP。
- 生成根是 `out/Debug` 与 `out/Release`。代理编译用 `build.bat debug`，不打开编译锁。

## 阶段 1 — 解释器归属接进 GN

现状（不要回退，也不要当成做完）：

- `session/cpython.h` / `cpython.cc` 已有 `app::py::Interpreter`、`interpreter()`、`InitHooks`。
- `src/plugin/runtime/python/runtime.cc` 的 `init` / `shutdown` / `eval` / `exec_source` / `exec_file` 已转给 `interpreter()`。插件仍拥有 `start` / `stop`、`append_smartgis_inittab`、模块卸载。`shutdown` 先卸模块再 `interpreter().shutdown()`。
- `glang.runtime/BUILD.gn` **没有** `:cpython`。插件 `BUILD.gn` 的 `:python` 把 `cpython.cc` 列进自己的 `sources`，`python312.lib` 挂在插件目标上。这条线还没有按锁定方式接上，Debug 桩编译也还没有验收。

- [ ] 在 `src/content/browser/glang.runtime/BUILD.gn` 增加 `source_set("cpython")`，sources 只有 `exec/session/cpython.cc` 与 `exec/session/cpython.h`。`import("//build/python.gni")`。`assert_no_deps` 挡住 `//src/app/*`（与同文件 `:session` 一致）。
- [ ] `python_enabled` 时，**只在** `:cpython` 上设置 `HAS_PYTHON`、`PYTHON_HOME`、`include_dirs` 的 Python 头、`libs += [ "python312.lib" ]`、`lib_dirs`。Debug（`is_debug`）走桩，不链该 lib。
- [ ] `src/plugin/runtime/python/BUILD.gn` 的 `:python` 去掉内嵌的 `cpython.cc` / `cpython.h`，改为 `deps` 加上 `//src/content/browser/glang.runtime:cpython`。插件目标不再自己链 `python312.lib`。
本段不要做：登记表、改 `run_suite_script`、改套件 `.py`、删 `smartgis`、动 `Op::kCount`。

### 门禁

命令：

```bat
build.bat debug script_main_test
```

`script_main_test`（`src/plugin/runtime/python/BUILD.gn`，`output_name = "script_main_test"`）`deps` `//src/plugin/runtime/python:python`。接线之后 `:python` 再 `deps` `//src/content/browser/glang.runtime:cpython`，同一次 Debug 构建会把两个目标都编进去。不要开编译锁。

通过（四条都要成立；只编过不够，因为今天 `cpython.cc` 还直接列在 `:python` 的 `sources` 里，同样能编过这个目标）：

1. 上述命令退出码 0。
2. `src/content/browser/glang.runtime/BUILD.gn` 有 `source_set("cpython")`，sources 只有 `exec/session/cpython.cc` 与 `exec/session/cpython.h`。`:python` 的 sources 不再列入这两个文件，`deps` 含该 `:cpython`，插件目标不再 `libs += [ "python312.lib" ]`。该 lib 只允许出现在 `:cpython` 上，且仅当 `python_enabled`。
3. `out\Debug\build.log` 不出现 `python312.lib`。`build/python.gni` 里 `python_enabled` 在 `is_debug` 时为 false。
4. `src/plugin/runtime/python/runtime.cc` 的 `PythonRuntime::init` 仍调用 `app::py::interpreter().init`（`HAS_PYTHON` 与桩两条路径都是）。插件翻译单元不调用 `Py_Initialize` / `Py_InitializeFromConfig`。

失败：`cpython.cc` 仍只编在插件目标里；或 Debug 链接了 `python312.lib`；或 `init` 不再走 `interpreter()`。

`out\Debug\script_main_test.exe` 的桩文案断言放在阶段 3。阶段 1 不把它当成已经切到 `main()` 执行器。

## 阶段 2 — `glang.plugin` 登记表

- [ ] 新增 `src/content/browser/glang.runtime/exec/session/plugin_fn.h` 与 `plugin_fn.cc`：`app::py::register_fn(name, params, eval)`、`unregister_fn`、按名查找。这张表不包含 `Python.h`，Debug 桩上也能测。GN 上单独成目标或挂到不依赖 `//src/app/*` 的 source_set。
- [ ] 用现有脚本名批量登记，不新增 `Op`：`op/document.cc`、`op/view.cc`、`op/horizon.cc`、`op/gesture.cc`、`op/plugin.cc`、`op/gis/**` 里的 `eval_*`。名字来自 `op/register.cc` 里 `OpDesc::name` 与别名（`fill_document_ops` / `fill_view_ops` / `fill_horizon_ops` / `fill_gesture_ops` / `fill_plugin_ops` / `fill_gis_ops` / `fill_buffer_op`）。`static_assert(Op::kCount == 105)` 保持。
- [ ] 解释器 `after_initialize` 安装 Python 模块 `glang.plugin`（仅 `HAS_PYTHON`）。调用路径只有按名查表。
- [ ] `src/plugin/runtime/python/script/attach.cc` 的 `script_dispatch` 查这张表，使旧的 `smartgis.script.<动词>` 仍到达同一个 `eval_*`。本段仍接受 `as_`，这样旧源码在阶段 3 改写之前还能跑。
本段不要做：不要把套件执行器换成 `exec_file`；不要改 `testing/tools/harness/**/*.py`；不要删 ANTLR / sema / lower。

### 门禁

阶段 1 的门禁未过，不得进入本段。

本段新增 `test("plugin_fn_test")`（`//testing/test.gni`，`output_name = "plugin_fn_test"`，用例是 gtest `TEST`）。登记表头不包含 `Python.h`。今天树上没有这个二进制；门禁命令用这个名字，不要再造第二个 runner。

```bat
build.bat debug plugin_fn_test
out\Debug\plugin_fn_test.exe
build.bat debug codegen_ir_test
out\Debug\codegen_ir_test.exe
```

通过：

1. `plugin_fn_test.exe` 退出码 0。断言：`mark`、`click`、以及 `fill_gis_ops` 里至少一个现有 `OpDesc::name` 能查到；未知名字查不到；测试进程不调用 `Py_Initialize`。
2. `codegen_ir_test.exe` 退出码 0，stdout 为 `codegen_ir_test: ok`。该程序覆盖 `test_parse_if_present`、`test_sema_errors`、`test_lower_slot_and_rclick`、`test_eval_stops`（parse / sema / lower / eval）。它 `deps` `:op`，因此 `op/register.cc` 的 `static_assert(Op::kCount == 105)` 仍须成立。
3. `content::run_suite_script`（`run_suite.cc`）仍是 `parse_python` → `check_suite` → `lower_program` → `eval_module`。这条函数没有单独的 gtest；本段以该文件未被改成 `exec_file`、且 `codegen_ir_test` 仍绿为准。

失败：三个名字里有查不到的，或未知名字被当成命中，或 `Op::kCount` 不再是 105，或套件执行器已经换成 `exec_file`。

## 阶段 3 — 套件执行 `main()`，再去掉 `as_`

- [ ] `src/content/browser/glang.runtime/run_suite.cc` 的 `content::run_suite_script` 改为 `app::py::interpreter().exec_file`（加载文件、调用 `main()`）。`session/run_script.cc` 的 `try_run_suite_script` / `run_suite_script_rc` 仍是原来的返回码包装。
- [ ] 失败是 Python 异常。未知动词在该调用抛出。此前语句的副作用保留（前缀 mark 可能已经写下）。这是相对 `check_suite` / `bind_method`「整文件在副作用之前失败」的行为变更，写进套件说明，不要假装语义没变。
- [ ] 门禁仍是各 suite `suite.json` 的 `required_marks`，外加有 BMP 时 `score_id == ok`。前缀 mark 不齐仍然失败。
- [ ] 然后改写 `testing/tools/harness/**/<id>.py`：返回值用变量，删掉 `as_`、`require_var`。样例是 `testing/tools/harness/browser/browser.input/browser.input.py` 的 `resolve_data(..., as_="china")`。路径规则不变：`testing/tools/harness/<family>/<id>/<id>.py`。
本段不要做：不要删 `language/front/`、`language/sema/`、`language/lower/`、`Op` 表；不要改 `PluginHost` 签名。

### 门禁

阶段 2 的门禁未过，不得进入本段。opcode 源可以还在树上，但不再是套件执行器。`codegen_ir_test` 在本段仍须绿（命令与通过条件同阶段 2）。

Debug 桩（不链 `python312.lib`）：

```bat
build.bat debug script_main_test
out\Debug\script_main_test.exe
```

通过：退出码 0，stdout 为 `script_main_test: ok`。现有断言：文件里已经有可调用的 `main` 时，`exec_file` 仍失败，错误文案含 `python not compiled in`。`out\Debug\build.log` 不含 `python312.lib`。

Release 才是真解释器。`python_enabled` 的定义在 `build/python.gni`：`has_python && python_prefix != "" && !is_debug`。本机没有 Python 3.12 前缀时，Release 仍是桩，本段不算过。

```bat
build.bat release script_main_test
out\Release\script_main_test.exe
build.bat release plugin_fn_test
out\Release\plugin_fn_test.exe
```

通过：两个程序退出码都是 0。`script_main_test` 的 stdout 为 `script_main_test: ok`（现有断言：可调用的 `main()` 能跑；`main` 里 `raise RuntimeError('boom-main')` 失败且文案含 `boom-main`）。不得把 Debug 的桩失败当成 Release 通过。

未知动词写成一条显式测试，不做成静默行为变化。在 `plugin_fn_test` 增加 `TEST(PluginFn, UnknownVerbThrowsAfterPrefixMark)`，只在 `HAS_PYTHON` 下编译。它先调用已登记的 `mark`，再调用未登记名字。通过条件：该调用抛异常，且 `mark` 的 `eval_*` 已经执行。禁止把终态改回 `check_suite` / `bind_method`「整份文件在任何副作用之前失败」。同一二进制在 Release 上还要覆盖升级后的调用面：`import glang.plugin` 之后调用 `mark` 与 `click`；`resolve_data(kind=..., leaf=...)` 用关键字参数，返回值是 `str`（不写 `as_` 槽）；`open_map` 接受这个返回值。宿主可以在运行期失败，但不能报 unknown verb，也不能在调用前被 sema 整文件拒绝。

套件命令先编 Release 产品，再按 id 跑。`build.bat release harness` 只编译（`harness` 目标带上 views / render），不跑套件。`build.bat` 里的 harness runner 只在 Debug 上执行，而且 `testing/tools/loop/gate.py` 的 `GATE_SUITE_IDS` 只有 `browser.gpu` 与 `browser.harness`。那不是本段门禁。

```bat
build.bat release harness
py -3 testing/tools/loop_runner.py --suite browser.input --no-build --out Release --rounds 1
py -3 testing/tools/loop_runner.py --suite browser.map2d.china --no-build --out Release --rounds 1
py -3 testing/tools/loop_runner.py --suite browser.console --no-build --out Release --rounds 1
py -3 testing/tools/loop_runner.py --suite browser.world3d.browse --no-build --out Release --rounds 1
```

四条 `loop_runner` 退出码都是 0 才算过。判定在 `testing/tools/loop/score/round.py`：`required_marks` 全部落地；`suite.json` 带 `bmp.score_id` 时 `score_bmp` 的 `ok` 为真。`visual_review` 不参与失败。前缀 mark 写了一部分、后面缺 mark 或 BMP 分数不是 ok，仍然失败。

| id | 本段要看到的结果 |
| --- | --- |
| `browser.input` | marks：`hwnd-ok`、`host-ok`、`input-point-ok`、`input-line-ok`、`input-poly-ok`、`input-ok`、`pass`。无 BMP。这是去掉 `as_` 的样例脚本。 |
| `browser.map2d.china` | marks：`china-ok`、`fit-ok`、`pass`、`extent-ok`。`score_id` `map2d_china_dem` 为 ok。 |
| `browser.console` | marks：`hwnd-ok`、`console-ok`、`pan-moved`、`console-bench-ok`、`pass`。无 BMP。 |
| `browser.world3d.browse` | 现有 `required_marks` 全部落地。`score_id` `plugin_scene3d` 为 ok。 |

其余浏览器场景与 11 个插件 suite 留在最终门禁，不在本段另写一条完成线。

## 阶段 4 — 产品 Python 包改为 `import glang.plugin`

已有 `kind=python` 样本：

- `src/plugin/runtime/python/samples/hello/`
- `src/plugin/runtime/python/samples/analysis/`
- `src/plugin/runtime/python/samples/product_orchestrate/`
- `src/plugin/runtime/python/samples/industry_pack/`

- [ ] 这些包的 `plugin.py` 改为 `import glang.plugin`。`start(host)` 仍贡献命令 / dock / processing，回调对象由 glang 持有。
- [ ] `bind/host.cc`、`bind/events.cc`、`bind/tool.cc`、`bind/ui.cc`、`bind/debug.cc` 与 `gis/analysis.cc`、`gis/scene.cc`、`gis/style.cc` 改成转发到 `register_fn`，然后停止加新的 `PyMethodDef`。
- [ ] `contribute_command`、`events.subscribe`、对话框工厂不再把裸 `PyObject*` 留在插件运行时。`stop`：先注销，再丢回调，再卸模块。
- [ ] 进 GIS 内核与 capture / `export_bmp` 前释放 GIL。

本段不要做：不要在本段删除 `smartgis` 模块文件；不要删 opcode 管线。

### 门禁

阶段 3 的门禁未过，不得进入本段。

```bat
build.bat release plugin_python_test
out\Release\plugin_python_test.exe
```

目标名是 `plugin_python_test`（sources 为 `python_test.cc`，`output_name = "plugin_python_test"`）。没有名为 `python_test` 的 GN 目标。

通过：

1. 退出码 0，stdout 为 `plugin_python_test: ok`。出现 `plugin_python_test: skip (no python)` 算失败：Debug 桩上 `init` 失败会打印这句并返回 0，不能当成本段通过。
2. hello 与 analysis 都经 `import glang.plugin` 被 `start`。今天该测试只 `start` `samples/hello`（`PLUGIN_HELLO_DIR`，断言 `hello sample start` / `hello sample.hello`），不启动 `samples/analysis`。本段把 analysis 的 `start` 断言补进同一个 `plugin_python_test`，不另造二进制。`product_orchestrate` 与 `industry_pack` 的 `plugin.py` 同样改为 `import glang.plugin`；现有测试没有启动它们，不把未写进该测试的样本说成已覆盖。
3. `git diff -- src/content/public/plugin_host.h` 为空。
4. `src/plugin/runtime/python/bind/` 与 `gis/` 相对本段开始不新增 `PyMethodDef`（改为转发 `register_fn`）。

`build.bat debug plugin_python_test` 仍须编过。它的 skip 退出码 0 不是本段通过条件。

## 阶段 5 — 转绿后删除 opcode 与 `smartgis`

前提：阶段 3 与阶段 4 的门禁都已绿。未绿不得删。

- [ ] 删除 ANTLR 前端：`glang.runtime/language/front/` 的 `parse_python`、`language/front/python/Python3*.g4`，以及 `BUILD.gn` 里的 `python3_lexer_gen` / `python3_parser_gen` / `:front` 中只服务该语法的部分。
- [ ] 删除 `language/sema/`（含 `check_suite`、`BoundProgram::slot_names`）与 `language/lower/`。
- [ ] 删除 `Op` 枚举、`exec/op/register.cc` 固定表、`exec/extend/language.h` 的 `LanguageExtension` 降级路径。`eval_*` 函数体留下，已经按名登记。
- [ ] 删除 `smartgis` 模块：`PyInit_smartgis`、`append_smartgis_inittab`、`bind/` 与 `gis/` 的方法表、`script/attach.cc` 的引导。插件 `BUILD.gn` 不再编这些绑定。
- [ ] 插件运行时只留清单、`start` / `stop`、`register_fn` / `unregister_fn`。进程里仍只有 `Interpreter` 调用 `Py_InitializeFromConfig` / `Py_FinalizeEx`。

本段不要做：不要改 `content/public`；不要把内核搬进 Python。套件与 `plugin_python_test` 的再验证只写在最终门禁，这里不另写一条完成线。

### 门禁

通过（三条都要成立）：

1. 下列源不再留在树上，`BUILD.gn` 不再引用：ANTLR `parse_python` 与 `language/front/python/Python3*.g4`（含 `python3_lexer_gen` / `python3_parser_gen`）；`language/sema/`（含 `check_suite`、`BoundProgram::slot_names`）；`language/lower/`；`Op` 枚举与 `exec/op/register.cc` 固定表；`exec/extend/language.h` 的 `LanguageExtension` 降级路径；`PyInit_smartgis`、`append_smartgis_inittab`、`bind/` 与 `gis/` 的方法表、`script/attach.cc` 的引导。`static_assert(Op::kCount == 105)` 随表消失。各 `eval_*` 函数体仍在，并已按名登记。
2. `test("codegen_ir_test")` 删除。`build.bat debug codegen_ir_test` 因目标不存在而失败。这是预期。此后语法门禁不再使用 ANTLR `check_suite` 或这个二进制。
3. Debug 桩仍能编能跑：`build.bat debug script_main_test` 退出码 0；`out\Debug\script_main_test.exe` stdout 为 `script_main_test: ok`（文案含 `python not compiled in`）；`out\Debug\build.log` 不含 `python312.lib`。

## 最终门禁

阶段 5 的门禁未过，不得把升级标成完成。Debug 桩（上一节第 3 条）仍然必须成立，但不能代替下面 (a) 和 (b)。(a) 与 (b) 都在 Release、且 `python_enabled` 为真时通过，最终门禁才算过。

### (a) 核心语法

三个 Release 二进制退出码都是 0，stdout 分别为 `script_main_test: ok`、gtest 全绿（`plugin_fn_test`）、`plugin_python_test: ok`。出现 `skip (no python)` 算失败。不使用 `codegen_ir_test`。

```bat
build.bat release script_main_test
build.bat release plugin_fn_test
build.bat release plugin_python_test
out\Release\script_main_test.exe
out\Release\plugin_fn_test.exe
out\Release\plugin_python_test.exe
```

| 二进制 | 必须覆盖的断言 |
| --- | --- |
| `script_main_test.exe` | `main()` 入口能跑；缺少可调用 `main` 时失败并点名 `main`；`main` 内异常文案含 `boom-main` |
| `plugin_fn_test.exe` | `import glang.plugin` 后调用 `mark`、`click`；`resolve_data(kind=..., leaf=...)` 关键字参数且返回 `str`；`open_map` 接受该返回值；`TEST(PluginFn, UnknownVerbThrowsAfterPrefixMark)`（前缀 `mark` 已执行，未知名字在该调用抛异常） |
| `plugin_python_test.exe` | `samples/hello` 与 `samples/analysis` 经 `import glang.plugin` 被 `start` |

### (b) 场景测试

先 `build.bat release harness` 编出 `out\Release\SmartGIS.exe`。该命令不跑套件。然后对下表每一个 id：

```bat
py -3 testing/tools/loop_runner.py --suite <id> --no-build --out Release --rounds 1
```

每条退出码 0。`required_marks` 全部落地；有 `bmp.score_id` 的 suite，`score_bmp` 的 `ok` 为真。`visual_review` 不参与失败。`plugin.report` 无 BMP，只看 marks（`hwnd-ok`、`report-ok`、`pass`）。

不要用 `build.bat debug harness` 或 `loop_runner.py --gate` 代替。(a) 没过也不能用场景结果充数。

浏览器场景（`testing/tools/harness/browser/<id>/suite.json` 的 `id` 与 `script` 已核对）：

| id | 脚本 | BMP `score_id` |
| --- | --- | --- |
| `browser.console` | `browser.console.py` | 无 |
| `browser.harness` | `browser.harness.py` | 无 |
| `browser.input` | `browser.input.py` | 无 |
| `browser.map2d.align` | `browser.map2d.align.py` | `map2d_align` |
| `browser.map2d.browse` | `browser.map2d.browse.py` | `map2d_china_carto` |
| `browser.map2d.china` | `browser.map2d.china.py` | `map2d_china_dem` |
| `browser.map2d.orthogrid` | `browser.map2d.orthogrid.py` | `map2d_orthogrid` |
| `browser.world3d.browse` | `browser.world3d.browse.py` | `plugin_scene3d` |
| `browser.world3d.full` | `browser.world3d.full.py` | `atmosphere_full`（同进程依次 land / ocean / coast / full；套件 BMP 仍是 full） |
| `browser.world3d.globe` | `browser.world3d.globe.py` | `atmosphere_globe` |
| `browser.world3d.legacy` | `browser.world3d.legacy.py` | `legacy_scene3d_china` |

`browser.gpu` 不在此列：`suite.json` 无 `script`，`probes` 为空，exe 是 `SmartGisRender.exe --self-test`。它只出现在现有 `--gate` 里。`ui.*` 不是本升级的场景集。

11 个插件 suite（`testing/tools/harness/plugin/<id>/<id>.py`，`id` 已核对）：

| id | BMP `score_id` |
| --- | --- |
| `plugin.flood` | `plugin_flood` |
| `plugin.geochem` | `plugin_map2d` |
| `plugin.traffic` | `plugin_map2d` |
| `plugin.orthogrid` | `map2d_orthogrid` |
| `plugin.print` | `plugin_print` |
| `plugin.report` | 无 |
| `plugin.mine` | `plugin_mesh` |
| `plugin.orthogrid3d` | `plugin_hex` |
| `plugin.stormsurge` | `plugin_stormsurge` |
| `plugin.world3d` | `plugin_scene3d` |
| `plugin.world3d.preview` | `plugin_product` |
