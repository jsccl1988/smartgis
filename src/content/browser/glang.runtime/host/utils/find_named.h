// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_UTILS_FIND_NAMED_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_UTILS_FIND_NAMED_H_

#include <string>

namespace content {
namespace detail {

// Depth-capped recursive file search under |root|. Used by the language
// source manager and by Host path slots. Not a CapabilityHost API.
bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_UTILS_FIND_NAMED_H_
