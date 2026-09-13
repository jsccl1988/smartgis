// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_raster_layer.h"

#include "sdb/datasource/gdal/gdal_driver.h"

#include "base/style/style_api.h"
#include "gdal_priv.h"
#include "cpl_vsi.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace sdb {
namespace datasource {
namespace detail {

constexpr char kImageCodeMeta[] = "SMT_IMAGE_CODE";

void rect_to_geotransform(const base::fRect& rect, int width, int height,
                          double* gt) {
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  gt[0] = rect.lb.x;
  gt[1] = (rect.rt.x - rect.lb.x) / static_cast<double>(w);
  gt[2] = 0.0;
  gt[3] = rect.rt.y;
  gt[4] = 0.0;
  gt[5] = (rect.lb.y - rect.rt.y) / static_cast<double>(h);
}

void geotransform_to_rect(const double* gt, int width, int height,
                          base::fRect* rect) {
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  rect->lb.x = static_cast<float>(gt[0]);
  rect->rt.y = static_cast<float>(gt[3]);
  rect->rt.x = static_cast<float>(gt[0] + gt[1] * w + gt[2] * h);
  rect->lb.y = static_cast<float>(gt[3] + gt[4] * w + gt[5] * h);
}

bool write_vsimem_blob(const char* path, const char* data, long size) {
  if (!path || size < 0) {
    return false;
  }
  VSIUnlink(path);
  VSILFILE* fp = VSIFOpenL(path, "wb");
  if (!fp) {
    return false;
  }
  const size_t n = size > 0 ? static_cast<size_t>(size) : 0;
  const size_t wrote =
      n == 0 ? 0 : VSIFWriteL(data, 1, n, fp);
  VSIFCloseL(fp);
  return wrote == n;
}

}  // namespace detail

OgrRasterLayer::OgrRasterLayer(GDALDataset* owner)
    : sdb::SmtRasterLayer(owner) {
  rect_.lb.x = 0;
  rect_.lb.y = 0;
  rect_.rt.x = 800;
  rect_.rt.y = 600;
  if (m_pOwnerDs && m_pOwnerDs->GetRasterCount() > 0) {
    sync_rect_from_dataset();
    m_bOpen = true;
  }
}

OgrRasterLayer::~OgrRasterLayer() {
  Close();
}

std::string OgrRasterLayer::blob_path() const {
  if (!vsimem_blob_.empty()) {
    return vsimem_blob_;
  }
  char buf[128];
  std::snprintf(buf, sizeof(buf), "/vsimem/sdb_ras_%p/blob",
                static_cast<const void*>(this));
  return std::string(buf);
}

void OgrRasterLayer::unlink_blob() {
  if (vsimem_blob_.empty()) {
    return;
  }
  VSIUnlink(vsimem_blob_.c_str());
  // Remove parent dir marker if present.
  const auto slash = vsimem_blob_.rfind('/');
  if (slash != std::string::npos && slash > 0) {
    VSIRmdir(vsimem_blob_.substr(0, slash).c_str());
  }
  vsimem_blob_.clear();
}

void OgrRasterLayer::release_owned_dataset() {
  if (owns_dataset_ && m_pOwnerDs) {
    GDALClose(m_pOwnerDs);
  }
  m_pOwnerDs = nullptr;
  owns_dataset_ = false;
}

void OgrRasterLayer::apply_geotransform() {
  if (!m_pOwnerDs) {
    return;
  }
  double gt[6] = {};
  detail::rect_to_geotransform(rect_, m_pOwnerDs->GetRasterXSize(),
                               m_pOwnerDs->GetRasterYSize(), gt);
  m_pOwnerDs->SetGeoTransform(gt);
}

void OgrRasterLayer::sync_rect_from_dataset() {
  if (!m_pOwnerDs || m_pOwnerDs->GetRasterCount() <= 0) {
    return;
  }
  double gt[6] = {};
  if (m_pOwnerDs->GetGeoTransform(gt) == CE_None) {
    detail::geotransform_to_rect(gt, m_pOwnerDs->GetRasterXSize(),
                                 m_pOwnerDs->GetRasterYSize(), &rect_);
  }
  const char* code = m_pOwnerDs->GetMetadataItem(detail::kImageCodeMeta);
  if (code && code[0]) {
    image_code_ = std::strtol(code, nullptr, 10);
  }
  CalEnvelope();
}

bool OgrRasterLayer::ensure_mem_dataset(int width, int height) {
  register_gdal_driver();
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("MEM");
  if (!mem) {
    return false;
  }
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  if (m_pOwnerDs && owns_dataset_ && m_pOwnerDs->GetRasterCount() > 0 &&
      m_pOwnerDs->GetRasterXSize() == w && m_pOwnerDs->GetRasterYSize() == h) {
    return true;
  }
  release_owned_dataset();
  char** options = nullptr;
  m_pOwnerDs = mem->Create("", w, h, 1, GDT_Byte, options);
  if (!m_pOwnerDs) {
    return false;
  }
  owns_dataset_ = true;
  apply_geotransform();
  return true;
}

bool OgrRasterLayer::Create() {
  if (m_bOpen && m_pOwnerDs && m_pOwnerDs->GetRasterCount() > 0) {
    return true;
  }
  if (!ensure_mem_dataset(1, 1)) {
    m_bOpen = false;
    return false;
  }
  m_bOpen = true;
  CalEnvelope();
  return true;
}

bool OgrRasterLayer::Open(const char* szLayerArchiveName) {
  if (!szLayerArchiveName || !szLayerArchiveName[0]) {
    return Create();
  }
  register_gdal_driver();
  release_owned_dataset();
  unlink_blob();
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      szLayerArchiveName, GDAL_OF_RASTER | GDAL_OF_UPDATE, nullptr, nullptr,
      nullptr));
  if (!ds || ds->GetRasterCount() <= 0) {
    if (ds) {
      GDALClose(ds);
    }
    m_bOpen = false;
    return false;
  }
  m_pOwnerDs = ds;
  owns_dataset_ = true;
  std::snprintf(m_szLayerName, MAX_LAYER_NAME, "%s", szLayerArchiveName);
  sync_rect_from_dataset();
  m_bOpen = true;
  return true;
}

bool OgrRasterLayer::Close() {
  unlink_blob();
  release_owned_dataset();
  image_code_ = -1;
  m_bOpen = false;
  return true;
}

bool OgrRasterLayer::Fetch(sdb::eSmtFetchType /*type*/) {
  return IsOpen();
}

void OgrRasterLayer::CalEnvelope() {
  ::rect_to_envelope(m_lyrEnv, rect_);
}

long OgrRasterLayer::SetRasterRect(const base::fRect& fLocRect) {
  rect_ = fLocRect;
  apply_geotransform();
  CalEnvelope();
  return SMT_ERR_NONE;
}

long OgrRasterLayer::CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                                   const base::fRect& fLocRect,
                                   long lImageCode) {
  if (lRasterBufSize < 0 || (lRasterBufSize > 0 && !pRasterBuf)) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (!IsOpen() && !Create()) {
    return SMT_ERR_FAILURE;
  }
  if (!m_pOwnerDs) {
    return SMT_ERR_FAILURE;
  }

  rect_ = fLocRect;
  image_code_ = lImageCode;
  apply_geotransform();

  vsimem_blob_ = blob_path();
  if (!detail::write_vsimem_blob(vsimem_blob_.c_str(), pRasterBuf,
                                lRasterBufSize)) {
    vsimem_blob_.clear();
    return SMT_ERR_FAILURE;
  }

  char code_buf[32];
  std::snprintf(code_buf, sizeof(code_buf), "%ld", lImageCode);
  m_pOwnerDs->SetMetadataItem(detail::kImageCodeMeta, code_buf);

  // Prefer GDAL-decoded bands when the blob is a known raster format.
  GDALDataset* decoded = static_cast<GDALDataset*>(GDALOpenEx(
      vsimem_blob_.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY, nullptr, nullptr,
      nullptr));
  if (decoded && decoded->GetRasterCount() > 0) {
    GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("MEM");
    GDALDataset* copy =
        mem ? mem->CreateCopy("", decoded, FALSE, nullptr, nullptr, nullptr)
            : nullptr;
    GDALClose(decoded);
    if (copy) {
      release_owned_dataset();
      m_pOwnerDs = copy;
      owns_dataset_ = true;
      apply_geotransform();
      m_pOwnerDs->SetMetadataItem(detail::kImageCodeMeta, code_buf);
    }
  } else if (decoded) {
    GDALClose(decoded);
  }

  CalEnvelope();
  return SMT_ERR_NONE;
}

long OgrRasterLayer::GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                               base::fRect& fLocRect,
                               long& lImageCode) const {
  char* src = nullptr;
  long size = 0;
  base::fRect loc;
  long code = 0;
  const long rc = GetRasterNoClone(src, size, loc, code);
  if (rc != SMT_ERR_NONE) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return rc;
  }
  if (size <= 0 || !src) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    fLocRect = loc;
    lImageCode = code;
    return SMT_ERR_NONE;
  }
  pRasterBuf = new char[static_cast<size_t>(size)];
  std::memcpy(pRasterBuf, src, static_cast<size_t>(size));
  lRasterBufSize = size;
  fLocRect = loc;
  lImageCode = code;
  return SMT_ERR_NONE;
}

long OgrRasterLayer::GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                                      base::fRect& fLocRect,
                                      long& lImageCode) const {
  fLocRect = rect_;
  lImageCode = image_code_;
  if (vsimem_blob_.empty()) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return SMT_ERR_NONE;
  }
  vsi_l_offset length = 0;
  GByte* data =
      VSIGetMemFileBuffer(vsimem_blob_.c_str(), &length, FALSE);
  if (!data) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return SMT_ERR_FAILURE;
  }
  pRasterBuf = reinterpret_cast<char*>(data);
  lRasterBufSize = static_cast<long>(length);
  return SMT_ERR_NONE;
}

long OgrRasterLayer::GetRasterRect(base::fRect& fLocRect) const {
  fLocRect = rect_;
  return SMT_ERR_NONE;
}

}  // namespace datasource
}  // namespace sdb
