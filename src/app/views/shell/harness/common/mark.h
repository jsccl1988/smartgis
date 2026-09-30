// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_MARK_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_MARK_H_

#include <windows.h>

namespace app {
namespace detail {

// Sidecar mark leaf names used by harness paths.
inline constexpr wchar_t kSelfTestMarkLeaf[] = L"self-test-mark.txt";
inline constexpr wchar_t kAtmosphereShowcaseMarkLeaf[] =
    L"atmosphere-showcase-mark.txt";
inline constexpr wchar_t kMap2dShowcaseMarkLeaf[] = L"map2d-showcase-mark.txt";
inline constexpr wchar_t kUiShowcaseMarkLeaf[] = L"ui-showcase-mark.txt";
inline constexpr wchar_t kInputShowcaseMarkLeaf[] =
    L"input-self-test-mark.txt";

// Deletes the sidecar mark file next to the exe (no-op if missing).
void clear_mark(const wchar_t* leaf);

// Appends |step| (+ newline) to the sidecar mark. If |truncate| is true on the
// first call for this leaf in-process, opens with "w"; otherwise "a".
void write_mark(const wchar_t* leaf, const char* step, bool truncate);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_MARK_H_
