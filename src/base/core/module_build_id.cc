// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Per-product-DLL build stamp. Injected by product_shared_library (GN).
// Keep this TU free of C++ objects that need unwind; no DllMain here.

#include "smartgis/build_id.h"

#if defined(COMPONENT_BUILD) && defined(_WIN32)
#define SMARTGIS_BUILD_ID_C_EXPORT extern "C" __declspec(dllexport)
#else
#define SMARTGIS_BUILD_ID_C_EXPORT extern "C"
#endif

// Materialize the full marker in .rdata for check_dll_homogeneity scans.
extern "C" const char smartgis_build_id_blob[] = SMARTGIS_BUILD_ID_MARKER;

SMARTGIS_BUILD_ID_C_EXPORT const char* smartgis_module_build_id(void) {
  return SMARTGIS_BUILD_ID;
}
