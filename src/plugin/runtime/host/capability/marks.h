// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_HOST_CAPABILITY_MARKS_H_
#define PLUGIN_RUNTIME_HOST_CAPABILITY_MARKS_H_

namespace plugin {

// Sidecar mark leaf basenames. Chrome `write_mark` routes them into
// out/<config>/captures/<scenario>/.
inline constexpr wchar_t kMarkHarness[] = L"harness-mark.txt";
inline constexpr wchar_t kMarkMap2d[] = L"map2d-showcase-mark.txt";
inline constexpr wchar_t kMarkPlugin[] = L"plugin-showcase-mark.txt";
inline constexpr wchar_t kMarkAtmosphere[] =
    L"atmosphere-showcase-mark.txt";

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_HOST_CAPABILITY_MARKS_H_
