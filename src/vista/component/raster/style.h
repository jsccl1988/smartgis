// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Style color pack for the software DIB. Opaque BGRA; alpha is forced on.

#ifndef VISTA_COMPONENT_RASTER_STYLE_H_
#define VISTA_COMPONENT_RASTER_STYLE_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>

namespace vista {
namespace raster {

inline uint32_t colorref_to_bgra(COLORREF c) {
  return 0xff000000u | (static_cast<uint32_t>(GetRValue(c)) << 16) |
         (static_cast<uint32_t>(GetGValue(c)) << 8) |
         static_cast<uint32_t>(GetBValue(c));
}

inline uint32_t rgba_to_bgra(uint32_t rgba) {
  return 0xff000000u | (static_cast<uint32_t>((rgba >> 16) & 0xff) << 16) |
         (static_cast<uint32_t>((rgba >> 8) & 0xff) << 8) |
         static_cast<uint32_t>(rgba & 0xff);
}

}  // namespace raster
}  // namespace vista

#endif  // VISTA_COMPONENT_RASTER_STYLE_H_
