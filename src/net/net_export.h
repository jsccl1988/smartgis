// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NET_NET_EXPORT_H_
#define NET_NET_EXPORT_H_

// GN defines NET_EXPORTS when building //src/base:base (dll_stem = base).
#if defined(NET_EXPORTS)
#define NET_EXPORT __declspec(dllexport)
#else
#define NET_EXPORT __declspec(dllimport)
#endif

#if !defined(NET_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  // NET_NET_EXPORT_H_
