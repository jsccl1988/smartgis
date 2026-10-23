// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_CONTENT_EXPORT_H_
#define CONTENT_CONTENT_EXPORT_H_

// GN defines CONTENT_EXPORTS when building the content DLL (dll_stem = content).
// When is_component_build=false, COMPONENT_BUILD is unset → no-op.

#if defined(COMPONENT_BUILD)
#if defined(CONTENT_EXPORTS)
#define CONTENT_EXPORT __declspec(dllexport)
#else
#define CONTENT_EXPORT __declspec(dllimport)
#endif
#else
#define CONTENT_EXPORT
#endif

#if defined(COMPONENT_BUILD) && !defined(CONTENT_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "content_d.lib")
#else
#pragma comment(lib, "content.lib")
#endif
#endif

#endif  // CONTENT_CONTENT_EXPORT_H_
