<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/simd`

Thin product face over the C++26-candidate SIMD library (P1928 / Parallelism TS).

| | |
| --- | --- |
| Include | `#include "base/simd/stdx.h"` |
| Namespace | `base::simd::stdx` → `vir::stdx` |
| Backend | `//third_party:vir_simd` ([mattkretz/vir-simd](https://github.com/mattkretz/vir-simd)) |
| GN | `//src/base/simd:simd` |

Do **not** use `<immintrin.h>` / raw `_mm*` in product TUs. Express batch work with
`stdx::native_simd` / `fixed_size_simd` and compile hot kernels with
`/arch:AVX2` so `native_simd<float>` is width 8 on x64.

**最后更新：** 2026-10-07
