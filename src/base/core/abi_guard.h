// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_ABI_GUARD_H_
#define BASE_CORE_ABI_GUARD_H_

// MSVC link-time guard against stale .obj (LNK2038 detect_mismatch).
//
// A — incremental half-rebuild inside one image:
//   Bump SMARTGIS_LAYOUT_EPOCH (or a type-local pragma in the header you
//   changed). Unrebuilt TUs keep the old token → link fails instead of
//   silently locking the wrong field as a mutex.
//
// B — cross-image mix is handled by SMARTGIS_BUILD_ID (module_build_id +
//   startup / artifact checks), not by detect_mismatch.
//
// See docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md
// §DLL / STL trust model (build consistency).

#ifndef SMARTGIS_LAYOUT_EPOCH
#define SMARTGIS_LAYOUT_EPOCH "2026-10-24.1"
#endif

#if defined(_MSC_VER)
#if defined(_DEBUG)
#pragma detect_mismatch("smartgis_crt", "mdd")
#else
#pragma detect_mismatch("smartgis_crt", "md")
#endif
#pragma detect_mismatch("smartgis_layout_epoch", SMARTGIS_LAYOUT_EPOCH)
#endif

#endif  // BASE_CORE_ABI_GUARD_H_
