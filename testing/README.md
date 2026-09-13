<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing

GN helpers for unit tests and (optional) benchmarks.

| File | Role |
| --- | --- |
| `test.gni` | `test("name")` → executable（gtest 尚未接入） |
| `benchmark.gni` | `benchmark("name")` → executable |

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

## End-to-end (product exes)

`testing/e2e/exe_smoke.cc` launches each chrome / GPU process with `--self-test`:

| Binary | What `--self-test` proves |
| --- | --- |
| `SmartGisRender.exe` | OOP GPU child + `FrameReady` + shared surface |
| `SmartGisViews.exe` | Views window + map attach (frame if render exe present) |
| `SmartGisWinui.exe` | WinUI window HWND |
| `SmartGis.exe` | MFC main frame created (harness closes if a modal keeps the pump busy) |

```bat
build.bat e2e
build.bat te
```

`e2e` sets the remaining `smt_build_*` flags, builds the chrome / GPU exes + `exe_smoke`, then runs `out\exe_smoke.exe --require-all` with cwd `out/`. `te` sets `smt_build_app` so leftover `SmartGis.exe` is rebuilt against the current GeoCore/GisCore ABI; other missing chrome exes are still skipped.

## Run

```bat
build.bat te
```

Aliases match mogu: `te` = `//:test_all`, `a` = `//:all_with_tests`, `b` = `//:benchmark_all`. Extra: `e2e` = `//:e2e`.

---

**最后更新：** 2026-09-13
