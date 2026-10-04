// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/core/log.h"
#include "scenic/render/rhi3d/impl/gl/host/render_device.h"
#include "scenic/render/rhi3d/impl/gl/resource/text/text.h"

#include <memory>

using namespace base;

namespace scenic {
namespace detail {
// print implement
long GlRenderDevice::CreateFont(const char *chType, int nHeight, int nWidth,
                                   int nWeight, bool bItalic, bool bUnderline,
                                   bool bStrike, ulong dwSize, uint &unID) {
  HDC hDC = m_hPaintDC ? m_hPaintDC : ::GetDC(m_hWnd);
  if (!hDC) {
    return kErrFailure;
  }

  auto text = std::make_unique<GlText>();
  if (kErrNone != text->CreateFont(hDC, chType, nHeight, nWidth, nWeight,
                                   bItalic, bUnderline, bStrike, dwSize)) {
    if (!m_hPaintDC) {
      ::ReleaseDC(m_hWnd, hDC);
    }
    return kErrFailure;
  }

  if (!m_hPaintDC) {
    ::ReleaseDC(m_hWnd, hDC);
  }

  m_vTextPtrs.push_back(std::move(text));
  unID = static_cast<uint>(m_vTextPtrs.size() - 1);

  return kErrNone;
}
}  // namespace detail
}  // namespace scenic