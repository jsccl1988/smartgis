// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef LEGACY_RENDER_GDI_BUFFER_IMAGE_H_
#define LEGACY_RENDER_GDI_BUFFER_IMAGE_H_

#include "scenic/render/err.h"
#include "scenic/render/rhi2d/public/device/render_device.h"

namespace scenic {
namespace detail {

class Rhi2dRenderDevice;

// Draw / stretch / save helpers over device render buffers (CxImage).
class Rhi2dBufferImage {
 public:
  explicit Rhi2dBufferImage(Rhi2dRenderDevice* device);

  int draw_image(const char* szImageBuf, int nImageBufSize, const fRect& frect,
                 long lCodeType, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
  int stretch_image(const char* szImageBuf, int nImageBufSize,
                    const fRect& frect, long lCodeType,
                    eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
  int save_image(const char* szFilePath, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                 bool bBgTransparent = false);
  int save2_image_buf(char*& szImageBuf, long& lImageBufSize, long lCodeType,
                      eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                      bool bBgTransparent = false);
  int free_image_buf(char*& szImageBuf);

 private:
  Rhi2dRenderDevice* device_;
};

}  // namespace detail
}  // namespace scenic

#endif  // LEGACY_RENDER_GDI_BUFFER_IMAGE_H_
