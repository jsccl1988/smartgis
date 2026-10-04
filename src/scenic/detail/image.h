// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_IMAGE_H_
#define SCENIC_DETAIL_IMAGE_H_

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

#include "ximage.h"

// CxImage format helper for scenic rhi2d buffer_image. Replaces
// legacy/core/util/image.h.

inline long get_image_type_by_file_ext(const char* szFileName) {
  if (szFileName == nullptr || szFileName[0] == '\0') {
    return CXIMAGE_FORMAT_UNKNOWN;
  }

  std::string lower(szFileName);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  const auto dot = lower.rfind('.');
  if (dot == std::string::npos || dot + 1 >= lower.size()) {
    return CXIMAGE_FORMAT_UNKNOWN;
  }
  const std::string ext = lower.substr(dot + 1);

  static const std::unordered_map<std::string, long> k_ext = {
      {"bmp", CXIMAGE_FORMAT_BMP},
      {"gif", CXIMAGE_FORMAT_GIF},
#if CXIMAGE_SUPPORT_JPG
      {"jpg", CXIMAGE_FORMAT_JPG},
      {"jpeg", CXIMAGE_FORMAT_JPG},
#endif
#if CXIMAGE_SUPPORT_PNG
      {"png", CXIMAGE_FORMAT_PNG},
#endif
#if CXIMAGE_SUPPORT_MNG
      {"mng", CXIMAGE_FORMAT_MNG},
#endif
#if CXIMAGE_SUPPORT_ICO
      {"ico", CXIMAGE_FORMAT_ICO},
#endif
#if CXIMAGE_SUPPORT_TIF
      {"tif", CXIMAGE_FORMAT_TIF},
      {"tiff", CXIMAGE_FORMAT_TIF},
#endif
#if CXIMAGE_SUPPORT_TGA
      {"tga", CXIMAGE_FORMAT_TGA},
#endif
#if CXIMAGE_SUPPORT_PCX
      {"pcx", CXIMAGE_FORMAT_PCX},
#endif
#if CXIMAGE_SUPPORT_WBMP
      {"wbmp", CXIMAGE_FORMAT_WBMP},
#endif
#if CXIMAGE_SUPPORT_WMF
      {"wmf", CXIMAGE_FORMAT_WMF},
#endif
#if CXIMAGE_SUPPORT_JPC
      {"jpc", CXIMAGE_FORMAT_JPC},
#endif
#if CXIMAGE_SUPPORT_JP2
      {"jp2", CXIMAGE_FORMAT_JP2},
#endif
#if CXIMAGE_SUPPORT_PGX
      {"pgx", CXIMAGE_FORMAT_PGX},
#endif
#if CXIMAGE_SUPPORT_PNM
      {"pnm", CXIMAGE_FORMAT_PNM},
#endif
#if CXIMAGE_SUPPORT_RAS
      {"ras", CXIMAGE_FORMAT_RAS},
#endif
  };

  const auto it = k_ext.find(ext);
  return it == k_ext.end() ? CXIMAGE_FORMAT_UNKNOWN : it->second;
}

#endif  // SCENIC_DETAIL_IMAGE_H_
