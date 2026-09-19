// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_text_encoding.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace gis {
namespace datasource {
namespace {

std::wstring wide_from_cp(unsigned cp, DWORD flags, const char* bytes) {
  if (!bytes || !bytes[0]) {
    return {};
  }
  const int n = MultiByteToWideChar(cp, flags, bytes, -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n - 1), L'\0');
  if (MultiByteToWideChar(cp, flags, bytes, -1, out.data(), n) <= 1) {
    return {};
  }
  return out;
}

}  // namespace

std::wstring ogr_bytes_to_wide(const char* bytes) {
  // Strict UTF-8 first so GeoJSON/GPKG stay correct. MB_ERR_INVALID_CHARS
  // is required: loose CP_UTF8 accepts GBK lead bytes and yields crumbs.
  std::wstring w = wide_from_cp(CP_UTF8, MB_ERR_INVALID_CHARS, bytes);
  if (!w.empty()) {
    return w;
  }
  w = wide_from_cp(936, 0, bytes);
  if (!w.empty()) {
    return w;
  }
  return wide_from_cp(CP_ACP, 0, bytes);
}

std::wstring ogr_bytes_to_wide(const std::string& bytes) {
  return ogr_bytes_to_wide(bytes.c_str());
}

std::string ogr_bytes_to_utf8(const char* bytes) {
  const std::wstring w = ogr_bytes_to_wide(bytes);
  if (w.empty()) {
    return {};
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr,
                      nullptr);
  return out;
}

std::string ogr_bytes_to_utf8(const std::string& bytes) {
  return ogr_bytes_to_utf8(bytes.c_str());
}

}  // namespace datasource
}  // namespace gis
