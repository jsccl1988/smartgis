// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_BAKE_BAKE_BACKEND_H_
#define VISTA_TERRAIN_DEM_BAKE_BAKE_BACKEND_H_

#include <cstdlib>
#include <string.h>

namespace vista {

// Equal-profile bake bench: BAKE_BACKEND / --bake-backend.
enum class BakeBackend {
  kAuto = 0,
  kCpu = 1,
  kCuda = 2,
};

inline BakeBackend parse_bake_backend_token(const char* v) {
  if (!v || !v[0]) {
    return BakeBackend::kAuto;
  }
  if (_stricmp(v, "cpu") == 0) {
    return BakeBackend::kCpu;
  }
  if (_stricmp(v, "cuda") == 0 || _stricmp(v, "gpu") == 0) {
    return BakeBackend::kCuda;
  }
  return BakeBackend::kAuto;
}

inline BakeBackend bake_backend_from_env() {
  return parse_bake_backend_token(std::getenv("BAKE_BACKEND"));
}

inline bool bake_disk_enabled_from_env() {
  const char* v = std::getenv("BAKE_DISK");
  if (v && v[0] == '0' && v[1] == '\0') {
    return false;
  }
  return true;
}

inline bool bake_bench_wanted_from_env() {
  const char* v = std::getenv("BAKE_BENCH");
  return v && v[0] == '1' && v[1] == '\0';
}

}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_BAKE_BAKE_BACKEND_H_
