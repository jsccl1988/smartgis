// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef TOOL_TOOL_EXPORT_H_
#define TOOL_TOOL_EXPORT_H_

// GN defines TOOL_EXPORTS when building //src/tool:tool (dll_stem = tool).
// Leftover IATool uses LEGACY_TOOL_EXPORT / LEGACY_TOOL_EXPORTS
// (//src/legacy/tool:legacy_tool). Link consumers via GN deps — no pragma lib.

#if defined(TOOL_EXPORTS)
#define TOOL_EXPORT __declspec(dllexport)
#else
#define TOOL_EXPORT __declspec(dllimport)
#endif

#endif  // TOOL_TOOL_EXPORT_H_
