// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_EXPORT_H_
#define PLUGIN_WORLD3D_EXPORT_H_

// Export for leftover plugin_dem.am (PLUGIN_DEM_EXPORTS) and any future
// plugin_world3d.dll. Outside those DLLs the macro is empty so loaders can
// also link into in-process tests.

#if defined(PLUGIN_WORLD3D_EXPORTS) || defined(PLUGIN_DEM_EXPORTS)
#define WORLD3D_LOADER_API __declspec(dllexport)
#else
#define WORLD3D_LOADER_API
#endif
// Compat for leftover TUs that still expand DEM_LOADER_API.
#ifndef DEM_LOADER_API
#define DEM_LOADER_API WORLD3D_LOADER_API
#endif

#endif  // PLUGIN_WORLD3D_EXPORT_H_
