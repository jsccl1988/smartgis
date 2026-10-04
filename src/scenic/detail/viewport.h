// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_VIEWPORT_H_
#define SCENIC_DETAIL_VIEWPORT_H_

// 2D viewport / windowport POD for scenic rhi2d. Replaces
// legacy/gis/present/carto/style_bas_struct.h for scenic_copy TUs.

namespace scenic {
namespace detail {

enum RenderBaseApi { kRdGdi = 0, kRdGdiplus, kRdSkia };

struct Viewport {
  float m_fVOX = 0;
  float m_fVOY = 0;
  float m_fVHeight = 0;
  float m_fVWidth = 0;
};

struct Windowport {
  float m_fWOX = 0;
  float m_fWOY = 0;
  float m_fWHeight = 0;
  float m_fWWidth = 0;
};

struct RenderOptions2d {
  bool bShowMBR = true;
  bool bShowPoint = true;
  long lPointRaduis = 2;
};

}  // namespace detail
}  // namespace scenic

// Historical names in namespace base (rhi2d `using namespace base`).
namespace base {
using RenderBaseApi = ::scenic::detail::RenderBaseApi;
inline constexpr RenderBaseApi RD_GDI = ::scenic::detail::kRdGdi;
inline constexpr RenderBaseApi RD_GDIPLUS = ::scenic::detail::kRdGdiplus;
inline constexpr RenderBaseApi RD_SKIA = ::scenic::detail::kRdSkia;
using Viewport = ::scenic::detail::Viewport;
using Windowport = ::scenic::detail::Windowport;
using RenderOptions2d = ::scenic::detail::RenderOptions2d;
}  // namespace base

#endif  // SCENIC_DETAIL_VIEWPORT_H_
