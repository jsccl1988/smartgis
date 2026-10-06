// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PATHS_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PATHS_H_

#include <cstddef>
#include <string>

#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// First existing relative path under the running exe directory, UTF-8 out.
bool resolve_first_existing_under_exe(const wchar_t* const* rels,
                                      size_t count,
                                      char* out_utf8,
                                      size_t out_cap);

// Binds resolve_data / capture_path / sidecar_path.
void bind_paths(content::CapabilityHost* out);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PATHS_H_
