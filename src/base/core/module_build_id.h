// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_MODULE_BUILD_ID_H_
#define BASE_CORE_MODULE_BUILD_ID_H_

#include "base/core/export.h"

// C export present in every product_shared_library image (component build).
// Name is fixed for GetProcAddress / artifact scans.
extern "C" const char* smartgis_module_build_id(void);

namespace base {

// Build id compiled into this image (base.dll / static).
BASE_EXPORT const char* module_build_id();

// Enumerate loaded product-stem modules; refuse mismatched SMARTGIS_BUILD_ID.
// Call once early in process startup (after prepare_dll_search_path).
// No-op success when is_component_build=false.
BASE_EXPORT bool verify_product_dll_build_ids();

}  // namespace base

#endif  // BASE_CORE_MODULE_BUILD_ID_H_
