// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UTIL_CHARSET_H_
#define APP_VIEWS_UTIL_CHARSET_H_

#include <cstddef>
#include <string>

namespace app {
namespace detail {

// UTF-8 <-> UTF-16 (wchar_t) conversions for the Views host. Overloads share
// one Win32 MultiByteToWideChar / WideCharToMultiByte implementation.

bool utf8_to_wide(const std::string& utf8, std::wstring* out);
std::wstring utf8_to_wide(const std::string& utf8);
std::wstring utf8_to_wide(const char* utf8);

bool wide_to_utf8(const std::wstring& wide, std::string* out);
std::string wide_to_utf8(const wchar_t* text);
bool wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UTIL_CHARSET_H_
