// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "gis/vista/frame/frame.h"

namespace content {
namespace detail {

// Rasters a MapFrame to an HDC (world items projected; pixel_space as-is).
void paint_map_frame_gdi(HDC hdc, const gis::vista::MapFrame& frame,
                         const gis::vista::View& view, bool fill_background);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_SOFTWARE_MAP2D_FRAME_GDI_H_
