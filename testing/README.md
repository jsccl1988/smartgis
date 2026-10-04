<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing

GN helpers for unit tests and google/benchmark targets.

| File | Role |
| --- | --- |
| `test.gni` | `test("name")` → executable（gtest 尚未接入） |
| `benchmark.gni` | `benchmark("name")` → executable + `//third_party:gbenchmark` (+ `gbenchmark_main` by default) |

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

分层方案（工具箱单测、`--self-test`、L2 壳像素、`exe` 冒烟）见
[`docs/superpowers/ui-testing.md`](../docs/superpowers/ui-testing.md)。L2 基线位于
[`src/ui/views/testdata/`](../src/ui/views/testdata/)。

## End-to-end (product exes)

`testing/e2e/exe_smoke.cc` launches each chrome / GPU process with `--self-test`:

| Binary | What `--self-test` proves |
| --- | --- |
| `SmartGisRender.exe` | OOP GPU child + `FrameReady` + shared surface |
| `SmartGIS.exe` | Views window + map attach (frame if render exe present) |
| `SmartGisWinui.exe` | WinUI window HWND |
| `SmartGIS-Legacy.exe` | MFC main frame created (harness closes if a modal keeps the pump busy) |

```bat
build.bat e2e
build.bat te
```

`e2e` sets the remaining `smt_build_*` flags, builds the chrome / GPU exes + `exe_smoke`, then runs `out\exe_smoke.exe --require-all` with cwd `out/`. `te` sets `smt_build_app` so leftover `SmartGIS-Legacy.exe` is rebuilt against the current GeoCore/GisCore ABI; other missing chrome exes are still skipped.

## Run

```bat
build.bat te
build.bat b
```

Aliases match mogu: `te` = `//:test_all`, `a` = `//:all_with_tests`, `b` = `//:benchmark_all`. Extra: `e2e` = `//:e2e`.

---

**最后更新：** 2026-09-28
