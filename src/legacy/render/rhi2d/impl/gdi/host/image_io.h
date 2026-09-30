// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef LEGACY_RENDER_GDI_IMAGE_IO_H_
#define LEGACY_RENDER_GDI_IMAGE_IO_H_

#include "legacy/core/macros/macros.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

namespace render {

class SmtGdiRenderDevice;

// Image draw / stretch / save helpers over device render buffers.
class GdiImageIo {
 public:
  explicit GdiImageIo(SmtGdiRenderDevice* device);

  int draw_image(const char* szImageBuf, int nImageBufSize, const fRect& frect,
                 long lCodeType, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
  int streth_image(const char* szImageBuf, int nImageBufSize,
                   const fRect& frect, long lCodeType,
                   eRDBufferLayer eMRDBufLyr = MRD_BL_MAP);
  int save_image(const char* szFilePath, eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                 bool bBgTransparent = false);
  int save2_image_buf(char*& szImageBuf, long& lImageBufSize, long lCodeType,
                      eRDBufferLayer eMRDBufLyr = MRD_BL_MAP,
                      bool bBgTransparent = false);
  int free_image_buf(char*& szImageBuf);

 private:
  SmtGdiRenderDevice* device_;
};

}  // namespace render

#endif  // LEGACY_RENDER_GDI_IMAGE_IO_H_
