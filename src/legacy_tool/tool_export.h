// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_TOOL_EXPORT_H_
#define TOOL_TOOL_EXPORT_H_

// GN defines TOOL_EXPORTS when building the tool DLL.
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

#endif  // TOOL_TOOL_EXPORT_H_
