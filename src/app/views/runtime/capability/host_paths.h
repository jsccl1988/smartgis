// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_RUNTIME_CAPABILITY_HOST_PATHS_H_
#define APP_VIEWS_RUNTIME_CAPABILITY_HOST_PATHS_H_

#include <string>

#include "content/browser/capability/host.h"

namespace app {
namespace detail {

// UTF-8 / wide helpers owned by the capability binder (not Interact IO).
std::wstring host_utf8_to_wide(const std::string& utf8);
bool host_wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap);

// Depth-capped recursive file search. Shared by suite script resolve.
bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth);

// Binds resolve_data / capture_path / sidecar_path. |out| must be non-null.
void bind_paths(content::CapabilityHost* out);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_RUNTIME_CAPABILITY_HOST_PATHS_H_
