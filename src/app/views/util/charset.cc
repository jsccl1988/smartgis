// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/util/charset.h"

#include <windows.h>

namespace app {
namespace detail {
namespace {

bool utf8_n_to_wide(const char* data, int nbytes, std::wstring* out) {
  if (!out) {
    return false;
  }
  if (!data || nbytes == 0) {
    out->clear();
    return true;
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, data, nbytes, nullptr, 0);
  if (n <= 0) {
    return false;
  }
  out->assign(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, data, nbytes, out->data(), n);
  if (nbytes < 0 && n > 0) {
    out->resize(static_cast<size_t>(n - 1));
  }
  return true;
}

bool wide_n_to_utf8(const wchar_t* data, int nchars, std::string* out) {
  if (!out) {
    return false;
  }
  if (!data || nchars == 0) {
    out->clear();
    return true;
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, data, nchars, nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) {
    return false;
  }
  out->assign(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, data, nchars, out->data(), n, nullptr,
                      nullptr);
  if (nchars < 0 && n > 0) {
    out->resize(static_cast<size_t>(n - 1));
  }
  return true;
}

}  // namespace

bool utf8_to_wide(const std::string& utf8, std::wstring* out) {
  if (!out) {
    return false;
  }
  if (utf8.empty()) {
    out->clear();
    return true;
  }
  return utf8_n_to_wide(utf8.data(), static_cast<int>(utf8.size()), out);
}

std::wstring utf8_to_wide(const std::string& utf8) {
  std::wstring out;
  if (!utf8_to_wide(utf8, &out)) {
    return {};
  }
  return out;
}

std::wstring utf8_to_wide(const char* utf8) {
  if (!utf8 || !utf8[0]) {
    return {};
  }
  std::wstring out;
  if (!utf8_n_to_wide(utf8, -1, &out)) {
    return {};
  }
  return out;
}

bool wide_to_utf8(const std::wstring& wide, std::string* out) {
  if (!out) {
    return false;
  }
  if (wide.empty()) {
    out->clear();
    return true;
  }
  return wide_n_to_utf8(wide.data(), static_cast<int>(wide.size()), out);
}

std::string wide_to_utf8(const wchar_t* text) {
  if (!text || !text[0]) {
    return {};
  }
  std::string out;
  if (!wide_n_to_utf8(text, -1, &out)) {
    return {};
  }
  return out;
}

bool wide_to_utf8(const wchar_t* wide, char* out, size_t out_cap) {
  if (!wide || !out || out_cap < 2) {
    return false;
  }
  return WideCharToMultiByte(CP_UTF8, 0, wide, -1, out,
                             static_cast<int>(out_cap), nullptr, nullptr) > 0;
}

}  // namespace detail
}  // namespace app
