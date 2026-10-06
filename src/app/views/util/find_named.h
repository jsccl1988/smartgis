// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UTIL_FIND_NAMED_H_
#define APP_VIEWS_UTIL_FIND_NAMED_H_

#include <string>

namespace app {
namespace detail {

// Depth-capped recursive file search under |root|. Used by the language
// source manager and by Host path slots. Not a CapabilityHost API.
bool find_named_under(const std::wstring& root,
                      const std::wstring& leaf,
                      std::wstring* out,
                      int depth);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UTIL_FIND_NAMED_H_
