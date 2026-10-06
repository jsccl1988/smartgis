// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_MARK_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_MARK_H_

#include <windows.h>

#include "plugin/runtime/host/capability/marks.h"

namespace app {
namespace detail {

// Sidecar mark leaf basenames. exe_capture_path routes them into
// captures/<scenario>/ (browser/, plugin/, ui/, legacy/, …).
inline constexpr auto kHarnessMarkLeaf = plugin::kMarkHarness;
// browser.world3d.browse must not share harness-mark.txt with map2d /
// harness / console — concurrent suites clobber orbit/wheel marks mid-score.
inline constexpr wchar_t kBrowse3dMarkLeaf[] = L"browse-3d-mark.txt";
inline constexpr auto kAtmosphereMarkLeaf =
    plugin::kMarkAtmosphere;
inline constexpr auto kMap2dMarkLeaf = plugin::kMarkMap2d;
inline constexpr auto kPluginMarkLeaf = plugin::kMarkPlugin;
inline constexpr wchar_t kUiMarkLeaf[] = L"ui-showcase-mark.txt";
inline constexpr wchar_t kInputMarkLeaf[] =
    L"input-self-test-mark.txt";

// Optional step logger used by present warmup, RHI session, and Scene3D capture.
using StepMarkFn = void (*)(const char* step);

inline void mark_step(StepMarkFn mark, const char* step) {
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
