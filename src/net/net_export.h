// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef NET_NET_EXPORT_H_
#define NET_NET_EXPORT_H_

// GN defines NET_EXPORTS when building the net DLL.
#if defined(NET_EXPORTS)
#define NET_EXPORT __declspec(dllexport)
#else
#define NET_EXPORT __declspec(dllimport)
#endif

#if !defined(NET_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "netD.lib")
#else
#pragma comment(lib, "net.lib")
#endif
#endif

#endif  // NET_NET_EXPORT_H_
