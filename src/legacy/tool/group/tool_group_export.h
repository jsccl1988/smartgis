// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_GROUP_TOOL_GROUP_EXPORT_H_
#define TOOL_GROUP_TOOL_GROUP_EXPORT_H_

// GN defines TOOL_GROUP_EXPORTS when building ui_legacy (tools + gui).
#if defined(TOOL_GROUP_EXPORTS)
#define TOOL_GROUP_EXPORT __declspec(dllexport)
#else
#define TOOL_GROUP_EXPORT __declspec(dllimport)
#endif

#if !defined(TOOL_GROUP_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "ui_legacy_d.lib")
#else
#pragma comment(lib, "ui_legacy.lib")
#endif
#endif

#endif  // TOOL_GROUP_TOOL_GROUP_EXPORT_H_
