// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-product-DLL build stamp. Injected by product_shared_library (GN).
// Keep this TU free of C++ objects that need unwind; no DllMain here.

#include <cstddef>

#include "smartgis/build_id.h"

#if defined(COMPONENT_BUILD) && defined(_WIN32)
#define SMARTGIS_BUILD_ID_C_EXPORT extern "C" __declspec(dllexport)
#else
#define SMARTGIS_BUILD_ID_C_EXPORT extern "C"
#endif

// Materialize the full marker in .rdata for check_dll_homogeneity scans.
// Must be referenced from an exported function: /OPT:REF otherwise drops the
// unused blob and the artifact gate reports "build_id marker missing".
extern "C" const char smartgis_build_id_blob[] = SMARTGIS_BUILD_ID_MARKER;

SMARTGIS_BUILD_ID_C_EXPORT const char* smartgis_module_build_id(void) {
  // Return the short id (suffix after "SMARTGIS_BUILD_ID=") so GetProcAddress
  // clients keep strcmp-compatible values; the full marker stays in .rdata.
  constexpr size_t kPrefix = sizeof("SMARTGIS_BUILD_ID=") - 1;
  return smartgis_build_id_blob + kPrefix;
}
