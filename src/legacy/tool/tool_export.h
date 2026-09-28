// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_TOOL_TOOL_EXPORT_H_
#define LEGACY_TOOL_TOOL_EXPORT_H_

// GN defines TOOL_EXPORTS when building legacy_tool (dll_stem = legacy_tool).

#if defined(TOOL_EXPORTS)
#define TOOL_EXPORT __declspec(dllexport)
#else
#define TOOL_EXPORT __declspec(dllimport)
#endif

#if !defined(TOOL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "legacy_tool_d.lib")
#else
#pragma comment(lib, "legacy_tool.lib")
#endif
#endif

#endif  // LEGACY_TOOL_TOOL_EXPORT_H_
