// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_MARK_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_MARK_H_

#include <windows.h>

#include "plugin/runtime/host/capability/marks.h"

namespace app {
namespace detail {

// Sidecar mark leaf basenames. exe_capture_path routes them into
// captures/<scenario>/ (shell/, plugin/, ui/, legacy/, …).
inline constexpr auto kHarnessMarkLeaf = plugin::kMarkHarness;
// browse.3d must not share harness-mark.txt with browse / console / harness
// — concurrent shell suites clobber orbit/wheel marks mid-score.
inline constexpr wchar_t kBrowse3dMarkLeaf[] = L"browse-3d-mark.txt";
inline constexpr auto kAtmosphereShowcaseMarkLeaf =
    plugin::kMarkAtmosphereShowcase;
inline constexpr auto kMap2dShowcaseMarkLeaf = plugin::kMarkMap2dShowcase;
inline constexpr auto kPluginShowcaseMarkLeaf = plugin::kMarkPluginShowcase;
inline constexpr wchar_t kUiShowcaseMarkLeaf[] = L"ui-showcase-mark.txt";
inline constexpr wchar_t kInputShowcaseMarkLeaf[] =
    L"input-self-test-mark.txt";

// Optional step logger used by present warmup, RHI session, and Scene3D capture.
using ShowcaseMarkFn = void (*)(const char* step);

inline void mark_step(ShowcaseMarkFn mark, const char* step) {
  if (mark && step) {
    mark(step);
  }
}

// Deletes the mark under out/<config>/captures/<scenario>/ (no-op if missing).
void clear_mark(const wchar_t* leaf);

// Appends |step| (+ newline) to the capture mark. If |truncate| is true on the
// first call for this leaf in-process, opens with "w"; otherwise "a".
void write_mark(const wchar_t* leaf, const char* step, bool truncate);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_MARK_H_
