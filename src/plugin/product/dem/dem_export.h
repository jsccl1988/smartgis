// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_DEM_DEM_EXPORT_H_
#define PLUGIN_PRODUCT_DEM_DEM_EXPORT_H_

// Export for plugin_dem.dll only. Callers outside that DLL see an empty
// macro so the loaders can also be compiled into in-process tests.

#if defined(PLUGIN_DEM_EXPORTS)
#define DEM_LOADER_API __declspec(dllexport)
#else
#define DEM_LOADER_API
#endif

#endif  // PLUGIN_PRODUCT_DEM_DEM_EXPORT_H_
