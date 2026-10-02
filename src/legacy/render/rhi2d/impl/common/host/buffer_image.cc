// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi2d/impl/common/host/buffer_image.h"

#include <cstdio>
#include <cstring>

#include "legacy/core/util/image.h"
#include "legacy/render/rhi2d/impl/common/host/render_device.h"
#include "legacy/render/rhi2d/impl/common/surface/dib/owned.h"
#include "ximage.h"

using namespace base;

namespace render {
namespace {

long draw_image_on_surface(Rhi2dOwnedSurface& surface, const char* image_buf,
                           int image_buf_size, long code_type, long x, long y,
                           long cx, long cy) {
  if (image_buf == nullptr || image_buf_size == 0) {
    return SMT_ERR_INVALID_PARAM;
  }
  CxImage tmp_image;
  tmp_image.Decode(reinterpret_cast<BYTE*>(const_cast<char*>(image_buf)),
                   image_buf_size, code_type);
  HDC hdc = surface.prepare_dc();
  tmp_image.Draw(hdc, x, y, cx, cy);
  surface.end_dc();
  return SMT_ERR_NONE;
}

long stretch_image_on_surface(Rhi2dOwnedSurface& surface, const char* image_buf,
                              int image_buf_size, long code_type, long xoffset,
                              long yoffset, long xsize, long ysize, DWORD rop) {
  if (image_buf == nullptr || image_buf_size == 0) {
    return SMT_ERR_INVALID_PARAM;
  }
  CxImage tmp_image;
  tmp_image.Decode(reinterpret_cast<BYTE*>(const_cast<char*>(image_buf)),
                   image_buf_size, code_type);
  HDC hdc = surface.prepare_dc();
  tmp_image.Stretch(hdc, xoffset, yoffset, xsize, ysize, rop);
  surface.end_dc();
  return SMT_ERR_NONE;
}

long save_bitmap_to_file(HBITMAP bitmap, const char* file_path,
                         bool bg_transparent) {
  if (bitmap == nullptr || file_path == nullptr ||
      std::strlen(file_path) == 0) {
    return SMT_ERR_FAILURE;
  }
  CxImage image;
  if (!image.CreateFromHBITMAP(bitmap)) {
    std::fprintf(stderr, "save_bitmap_to_file: CreateFromHBITMAP failed\n");
    return SMT_ERR_FAILURE;
  }
  if (bg_transparent) {
    const COLORREF bg_clr = RGB(255, 255, 255);
    RGBQUAD trans_clr;
    trans_clr.rgbRed = GetRValue(bg_clr);
    trans_clr.rgbGreen = GetGValue(bg_clr);
    trans_clr.rgbBlue = GetBValue(bg_clr);
    trans_clr.rgbReserved = 0;
    image.SetTransIndex(0);
    image.SetTransColor(trans_clr);
  }
  if (image.Save(file_path, get_image_type_by_file_ext(file_path))) {
    return SMT_ERR_NONE;
  }
  std::fprintf(stderr, "save_bitmap_to_file: CxImage::Save failed path=%s\n",
               file_path);
  return SMT_ERR_FAILURE;
}

long save_bitmap_to_buf(HBITMAP bitmap, char*& image_buf, long& image_buf_size,
                        long code_type, bool bg_transparent) {
  if (bitmap == nullptr || image_buf != nullptr) {
    return SMT_ERR_FAILURE;
  }
  uint8_t* encoded = nullptr;
  int32_t size = 0;
  CxImage image;
  if (!image.CreateFromHBITMAP(bitmap)) {
    return SMT_ERR_FAILURE;
  }
  if (bg_transparent) {
    const COLORREF bg_clr = RGB(255, 255, 255);
    RGBQUAD trans_clr;
    trans_clr.rgbRed = GetRValue(bg_clr);
    trans_clr.rgbGreen = GetGValue(bg_clr);
    trans_clr.rgbBlue = GetBValue(bg_clr);
    trans_clr.rgbReserved = 0;
    image.SetTransIndex(0);
    image.SetTransColor(trans_clr);
  }
  if (image.Encode(encoded, size, static_cast<uint32_t>(code_type))) {
    image_buf = reinterpret_cast<char*>(encoded);
    image_buf_size = size;
    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}

}  // namespace

Rhi2dBufferImage::Rhi2dBufferImage(SmtRhi2dRenderDevice* device) : device_(device) {}

int Rhi2dBufferImage::draw_image(const char* szImageBuf, int nImageBufSize,
                               const fRect& frect, long lCodeType,
                               eRDBufferLayer eMRDBufLyr) {
  Rhi2dOwnedSurface* surface = nullptr;
  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      surface = &device_->map_front_;
      break;
    case MRD_BL_DYNAMIC:
      surface = &device_->dynamic_buf_;
      break;
    case MRD_BL_QUICK:
      surface = &device_->raster_back_;
      break;
    default:
      return SMT_ERR_FAILURE;
  }
  lRect lrt;
  device_->LRectToDRect(frect, lrt);
  return draw_image_on_surface(*surface, szImageBuf, nImageBufSize, lCodeType,
                               lrt.lb.x, lrt.rt.y, lrt.width(), lrt.height());
}

int Rhi2dBufferImage::stretch_image(const char* szImageBuf, int nImageBufSize,
                                  const fRect& frect, long lCodeType,
                                  eRDBufferLayer eMRDBufLyr) {
  Rhi2dOwnedSurface* surface = nullptr;
  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      surface = &device_->map_front_;
      break;
    case MRD_BL_DYNAMIC:
      surface = &device_->dynamic_buf_;
      break;
    case MRD_BL_QUICK:
      surface = &device_->raster_back_;
      break;
    default:
      return SMT_ERR_FAILURE;
  }
  lRect lrt;
  device_->LRectToDRect(frect, lrt);
  return stretch_image_on_surface(*surface, szImageBuf, nImageBufSize,
                                  lCodeType, lrt.lb.x, lrt.rt.y, lrt.width(),
                                  lrt.height(), SRCCOPY);
}

int Rhi2dBufferImage::save_image(const char* szFilePath,
                               eRDBufferLayer eMRDBufLyr, bool bBgTransparent) {
  Rhi2dOwnedSurface* surface = nullptr;
  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      surface = &device_->map_front_;
      break;
    case MRD_BL_DYNAMIC:
      surface = &device_->dynamic_buf_;
      break;
    case MRD_BL_QUICK:
      surface = &device_->raster_back_;
      break;
    default:
      return SMT_ERR_FAILURE;
  }
  if (surface->bitmap() == nullptr) {
    std::fprintf(stderr,
                 "Rhi2dBufferImage::save_image null bitmap layer=%d w=%d h=%d\n",
                 static_cast<int>(eMRDBufLyr), surface->width(),
                 surface->height());
    return SMT_ERR_FAILURE;
  }
  // CreateFromHBITMAP fails while the DIB is selected into paint_dc_.
  (void)surface->end_dc();
  return save_bitmap_to_file(surface->bitmap(), szFilePath, bBgTransparent);
}

int Rhi2dBufferImage::save2_image_buf(char*& szImageBuf, long& lImageBufSize,
                                    long lCodeType, eRDBufferLayer eMRDBufLyr,
                                    bool bBgTransparent) {
  Rhi2dOwnedSurface* surface = nullptr;
  switch (eMRDBufLyr) {
    case MRD_BL_MAP:
      surface = &device_->map_front_;
      break;
    case MRD_BL_DYNAMIC:
      surface = &device_->dynamic_buf_;
      break;
    case MRD_BL_QUICK:
      surface = &device_->raster_back_;
      break;
    default:
      return SMT_ERR_FAILURE;
  }
  return save_bitmap_to_buf(surface->bitmap(), szImageBuf, lImageBufSize,
                            lCodeType, bBgTransparent);
}

int Rhi2dBufferImage::free_image_buf(char*& szImageBuf) {
  // Encode() allocates with malloc; must free via CxImage::FreeMemory (not
  // delete[]), or the CRT heap cookie / later paints AV (0xC0000409).
  if (szImageBuf) {
    CxImage tmp;
    tmp.FreeMemory(szImageBuf);
    szImageBuf = nullptr;
  }
  return SMT_ERR_NONE;
}

}  // namespace render
