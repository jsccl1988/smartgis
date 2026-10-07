<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# vir-simd

Header-only [mattkretz/vir-simd](https://github.com/mattkretz/vir-simd) — portable
polyfill for Parallelism TS / C++26 `std::simd` (P1928) as `vir::stdx::simd`.

| | |
| --- | --- |
| Pin | `v0.4.4` in `third_party/manifest.json` |
| Sources | `third_party/.src/vir-simd` |
| GN | `//third_party:vir_simd` |
| Product face | `#include "base/simd/stdx.h"` |

MSVC has no `<simd>` yet; vir-simd supplies `fixed_size` / `native` ABI tags.
Compile hot TUs with `/arch:AVX2` so `native_simd<float>` is width 8.

**最后更新：** 2026-10-07
