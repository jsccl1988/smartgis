<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing

GN helpers for unit tests and google/benchmark targets. Product runtime
gate lives under **`testing/tools`** (Views harness + plugin IL), not a
parallel `testing/e2e` tree.

| File | Role |
| --- | --- |
| `test.gni` | `test("name")` → executable + gtest (unless opted out) |
| `benchmark.gni` | `benchmark("name")` → executable + `//third_party:gbenchmark` (+ `gbenchmark_main` by default) |
| `tools/` | Harness suite loops (`loop_runner.py`, `harness/<family>/<id>/`) |

## Register a benchmark

```gn
import("//testing/benchmark.gni")

benchmark("buffer_benchmark") {
  sources = [ "ops/buffer_benchmark.cc" ]
  deps = [ ":geo" ]
}
```

Custom `main` (bootstrap / env skip): set `use_benchmark_main = false` and call
`benchmark::Initialize` / `RunSpecifiedBenchmarks` yourself.

JSON / console reporters: pass `--benchmark_format=json --benchmark_out=path`
(or `SG_CONSOLE_BENCH_JSON` for `content_console_bench`).

Then add `"//<module>:<name>"` to root `//:benchmark_all` (`BUILD.gn`).

## Register a test

In the module `BUILD.gn`:

```gn
import("//testing/test.gni")

test("core_test") {
  sources = [ "core_unittest.cpp" ]
  deps = [ "//src/base:core" ]
}
```

Then add `"//<module>:<name>"` to root `//:test_all` (`BUILD.gn`).

## GUI / Views testing

分层方案、进程内单测、`--harness`（别名 `--self-test`）、L2 像素金样见
[`docs/superpowers/ui-testing.md`](../docs/superpowers/ui-testing.md)。L2 金样位在
[`src/ui/views/testdata/`](../src/ui/views/testdata/)。

## Product runtime (harness)

`testing/tools/loop_runner.py --gate` is the product PE gate (replaces
`exe_smoke` / `testing/e2e`):

| Suite | Binary / flags | What it proves |
| --- | --- | --- |
| `gpu` | `SmartGisRender.exe --self-test` | OOP GPU child + D3D device + shared surface |
| `harness` | `SmartGIS.exe --harness` | Views window + map/3D attach + GIS plugin pipeline marks |

Showcase / plugin IL suites (world3d, mine, orthogrid, print, report,
stormsurge, traffic, flood, geochem, map2d, atmosphere, ui, shell) live
under `testing/tools/harness/<family>/<id>/` (`plugin/` owns `--plugin-showcase`
suites; `browser/` owns map2d and atmosphere) and share ids with C++ `ScenarioRegistry`.

```bat
build.bat debug harness
build.bat debug te
py -3 testing\tools\loop_runner.py --gate --no-build
py -3 testing\tools\loop_runner.py --list
py -3 testing\tools\loop_runner.py --suite plugin.world3d --no-build
```

`harness` (also accepted as `e2e`) sets `build_views` + `build_render`,
builds `//:harness`, then runs the gate. `te` builds `//:test_all` and
runs listed `*_test.exe` only (no PE smoke).

## Run

```bat
build.bat te
build.bat b
```

Aliases: `te` = `//:test_all`, `a` = `//:all_with_tests`, `b` = `//:benchmark_all`,
`harness` = `//:harness` + `loop_runner --gate`. `e2e` is a synonym of `harness`.

---

**最后更新：** 2026-10-06
