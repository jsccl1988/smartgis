// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RENDER_BACKEND_DLL_H_
#define SCENIC_RENDER_BACKEND_DLL_H_

#include <cstddef>
#include <cstdio>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/core/log.h"

// Shared LoadLibrary helper for Renderer2d / Renderer3d. Not a leftover
// dump dir — lives next to rhi2d/ and rhi3d/.

namespace scenic {
namespace detail {

struct BackendDllSpec {
  const char* api;
  const char* dll_stem;
  const char* create_export;
};

template <std::size_t N>
inline const BackendDllSpec* find_backend_spec(
    const BackendDllSpec (&specs)[N], const char* api) {
  if (!api) {
    return nullptr;
  }
  for (std::size_t i = 0; i < N; ++i) {
    if (std::strcmp(api, specs[i].api) == 0) {
      return &specs[i];
    }
  }
  return nullptr;
}

inline HMODULE load_backend_dll(const char* stem) {
  char name[64];
#ifdef _DEBUG
  _snprintf(name, sizeof(name), "%s_d.dll", stem);
#else
  _snprintf(name, sizeof(name), "%s.dll", stem);
#endif
  HMODULE dll = ::LoadLibraryA(name);
  if (!dll) {
    LOGGING(LOG_ERROR, "Loading %s failed (GetLastError=%lu).", name,
            static_cast<unsigned long>(::GetLastError()));
  }
  return dll;
}

inline void unload_backend_dll(HMODULE* dll) {
  if (dll && *dll) {
    ::FreeLibrary(*dll);
    *dll = nullptr;
  }
}

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RENDER_BACKEND_DLL_H_
