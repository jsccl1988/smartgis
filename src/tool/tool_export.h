// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_TOOL_EXPORT_H_
#define TOOL_TOOL_EXPORT_H_

// GN defines SMT_TOOL_EXPORTS when building //src/tool:tool (dll_stem = tool).
// Do not reuse TOOL_EXPORTS / TOOL_EXPORT — those belong to
// //src/legacy/tool:legacy_tool (dll_stem = legacy_tool).

#if defined(SMT_TOOL_EXPORTS)
#define SMT_TOOL_EXPORT __declspec(dllexport)
#else
#define SMT_TOOL_EXPORT __declspec(dllimport)
#endif

#if !defined(SMT_TOOL_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "tool_d.lib")
#else
#pragma comment(lib, "tool.lib")
#endif
#endif

#endif  // TOOL_TOOL_EXPORT_H_
