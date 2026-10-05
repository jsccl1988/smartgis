// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/ogr/ogr_raster_layer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "cpl_vsi.h"
#include "gdal_priv.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "ximage.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace gis {
namespace datasource {
namespace {

long image_code_from_path(const char* path) {
  if (!path || !path[0]) {
    return CXIMAGE_FORMAT_UNKNOWN;
  }
  std::string lower(path);
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
#if CXIMAGE_SUPPORT_TIF
      {"tif", CXIMAGE_FORMAT_TIF},
      {"tiff", CXIMAGE_FORMAT_TIF},
#endif
  };
  const auto it = k_ext.find(ext);
  return it == k_ext.end() ? CXIMAGE_FORMAT_UNKNOWN : it->second;
}

}  // namespace

namespace detail {

constexpr char kImageCodeMeta[] = "IMAGE_CODE";

void rect_to_geotransform(const Envelope& rect, int width, int height,
                          double* gt) {
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  gt[0] = rect.MinX;
  gt[1] = (rect.MaxX - rect.MinX) / static_cast<double>(w);
  gt[2] = 0.0;
  gt[3] = rect.MaxY;
  gt[4] = 0.0;
  gt[5] = (rect.MinY - rect.MaxY) / static_cast<double>(h);
}

void geotransform_to_rect(const double* gt, int width, int height,
                          Envelope* rect) {
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  rect->MinX = gt[0];
  rect->MaxY = gt[3];
  rect->MaxX = gt[0] + gt[1] * w + gt[2] * h;
  rect->MinY = gt[3] + gt[4] * w + gt[5] * h;
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
  const size_t wrote = n == 0 ? 0 : VSIFWriteL(data, 1, n, fp);
  VSIFCloseL(fp);
  return wrote == n;
}

}  // namespace detail

OgrRasterLayer::OgrRasterLayer(GDALDataset* owner) : owner_ds_(owner) {
  rect_.MinX = 0;
  rect_.MinY = 0;
  rect_.MaxX = 800;
  rect_.MaxY = 600;
  if (owner_ds_ && owner_ds_->GetRasterCount() > 0) {
    sync_rect_from_dataset();
    open_ = true;
  }
}

OgrRasterLayer::~OgrRasterLayer() { close(); }

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
  if (owns_dataset_ && owner_ds_) {
    GDALClose(owner_ds_);
  }
  owner_ds_ = nullptr;
  owns_dataset_ = false;
}

void OgrRasterLayer::apply_geotransform() {
  if (!owner_ds_) {
    return;
  }
  double gt[6] = {};
  detail::rect_to_geotransform(rect_, owner_ds_->GetRasterXSize(),
                               owner_ds_->GetRasterYSize(), gt);
  owner_ds_->SetGeoTransform(gt);
}

void OgrRasterLayer::sync_rect_from_dataset() {
  if (!owner_ds_ || owner_ds_->GetRasterCount() <= 0) {
    return;
  }
  double gt[6] = {};
  if (owner_ds_->GetGeoTransform(gt) == CE_None) {
    detail::geotransform_to_rect(gt, owner_ds_->GetRasterXSize(),
                                 owner_ds_->GetRasterYSize(), &rect_);
  }
  const char* code = owner_ds_->GetMetadataItem(detail::kImageCodeMeta);
  if (code && code[0]) {
    image_code_ = std::strtol(code, nullptr, 10);
  }
  cal_envelope();
}

void OgrRasterLayer::backfill_blob_from_path(const char* path) {
  if (!path || !path[0]) {
    return;
  }
  VSILFILE* fp = VSIFOpenL(path, "rb");
  if (!fp) {
    return;
  }
  if (VSIFSeekL(fp, 0, SEEK_END) != 0) {
    VSIFCloseL(fp);
    return;
  }
  const vsi_l_offset length = VSIFTellL(fp);
  if (VSIFSeekL(fp, 0, SEEK_SET) != 0 || length == 0 ||
      length > static_cast<vsi_l_offset>(64 * 1024 * 1024)) {
    VSIFCloseL(fp);
    return;
  }
  std::vector<char> buf(static_cast<size_t>(length));
  const size_t read = VSIFReadL(buf.data(), 1, static_cast<size_t>(length), fp);
  VSIFCloseL(fp);
  if (read != static_cast<size_t>(length)) {
    return;
  }
  vsimem_blob_ = blob_path();
  if (!detail::write_vsimem_blob(vsimem_blob_.c_str(), buf.data(),
                                 static_cast<long>(length))) {
    vsimem_blob_.clear();
    return;
  }
  if (image_code_ < 0) {
    std::string mutable_path = path;
    image_code_ = image_code_from_path(mutable_path.data());
  }
  if (owner_ds_) {
    char code_buf[32];
    std::snprintf(code_buf, sizeof(code_buf), "%ld", image_code_);
    owner_ds_->SetMetadataItem(detail::kImageCodeMeta, code_buf);
  }
}

bool OgrRasterLayer::ensure_mem_dataset(int width, int height) {
  register_gdal_driver();
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("MEM");
  if (!mem) {
    return false;
  }
  const int w = width > 0 ? width : 1;
  const int h = height > 0 ? height : 1;
  if (owner_ds_ && owns_dataset_ && owner_ds_->GetRasterCount() > 0 &&
      owner_ds_->GetRasterXSize() == w && owner_ds_->GetRasterYSize() == h) {
    return true;
  }
  release_owned_dataset();
  char** options = nullptr;
  owner_ds_ = mem->Create("", w, h, 1, GDT_Byte, options);
  if (!owner_ds_) {
    return false;
  }
  owns_dataset_ = true;
  apply_geotransform();
  return true;
}

bool OgrRasterLayer::create() {
  if (open_ && owner_ds_ && owner_ds_->GetRasterCount() > 0) {
    return true;
  }
  if (!ensure_mem_dataset(1, 1)) {
    open_ = false;
    return false;
  }
  open_ = true;
  cal_envelope();
  return true;
}

bool OgrRasterLayer::open(const char* szLayerArchiveName) {
  if (!szLayerArchiveName || !szLayerArchiveName[0]) {
    return create();
  }
  register_gdal_driver();
  release_owned_dataset();
  unlink_blob();
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx(szLayerArchiveName, GDAL_OF_RASTER | GDAL_OF_UPDATE, nullptr,
                 nullptr, nullptr));
  if (!ds || ds->GetRasterCount() <= 0) {
    if (ds) {
      GDALClose(ds);
    }
    open_ = false;
    return false;
  }
  owner_ds_ = ds;
  owns_dataset_ = true;
  name_ = szLayerArchiveName;
  sync_rect_from_dataset();
  // GDI/CxImage still consume encoded blobs via get_raster_no_clone. Copy the
  // source file into /vsimem so open(path) matches create_raster semantics.
  backfill_blob_from_path(szLayerArchiveName);
  open_ = true;
  return true;
}

bool OgrRasterLayer::close() {
  unlink_blob();
  release_owned_dataset();
  image_code_ = -1;
  open_ = false;
  return true;
}

bool OgrRasterLayer::fetch() { return is_open(); }

void OgrRasterLayer::cal_envelope() { envelope_ = rect_; }

void OgrRasterLayer::set_name(const char* szName) {
  name_ = szName ? szName : "";
}

void OgrRasterLayer::set_rect(const Envelope& lyr_rect) {
  set_raster_rect(lyr_rect);
}

long OgrRasterLayer::set_raster_rect(const Envelope& fLocRect) {
  rect_ = fLocRect;
  apply_geotransform();
  cal_envelope();
  return k_raster_ok;
}

long OgrRasterLayer::create_raster(const char* pRasterBuf, long lRasterBufSize,
                                   const Envelope& fLocRect,
                                   long lImageCode) {
  if (lRasterBufSize < 0 || (lRasterBufSize > 0 && !pRasterBuf)) {
    return k_raster_invalid;
  }
  if (!is_open() && !create()) {
    return k_raster_fail;
  }
  if (!owner_ds_) {
    return k_raster_fail;
  }

  rect_ = fLocRect;
  image_code_ = lImageCode;
  apply_geotransform();

  vsimem_blob_ = blob_path();
  if (!detail::write_vsimem_blob(vsimem_blob_.c_str(), pRasterBuf,
                                 lRasterBufSize)) {
    vsimem_blob_.clear();
    return k_raster_fail;
  }

  char code_buf[32];
  std::snprintf(code_buf, sizeof(code_buf), "%ld", lImageCode);
  owner_ds_->SetMetadataItem(detail::kImageCodeMeta, code_buf);

  // Prefer GDAL-decoded bands when the blob is a known raster format.
  GDALDataset* decoded = static_cast<GDALDataset*>(
      GDALOpenEx(vsimem_blob_.c_str(), GDAL_OF_RASTER | GDAL_OF_READONLY,
                 nullptr, nullptr, nullptr));
  if (decoded && decoded->GetRasterCount() > 0) {
    GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("MEM");
    GDALDataset* copy =
        mem ? mem->CreateCopy("", decoded, FALSE, nullptr, nullptr, nullptr)
            : nullptr;
    GDALClose(decoded);
    if (copy) {
      release_owned_dataset();
      owner_ds_ = copy;
      owns_dataset_ = true;
      apply_geotransform();
      owner_ds_->SetMetadataItem(detail::kImageCodeMeta, code_buf);
    }
  } else if (decoded) {
    GDALClose(decoded);
  }

  cal_envelope();
  return k_raster_ok;
}

long OgrRasterLayer::get_raster(char*& pRasterBuf, long& lRasterBufSize,
                               Envelope& fLocRect, long& lImageCode) const {
  char* src = nullptr;
  long size = 0;
  Envelope loc;
  long code = 0;
  const long rc = get_raster_no_clone(src, size, loc, code);
  if (rc != k_raster_ok) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return rc;
  }
  if (size <= 0 || !src) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    fLocRect = loc;
    lImageCode = code;
    return k_raster_ok;
  }
  pRasterBuf = new char[static_cast<size_t>(size)];
  std::memcpy(pRasterBuf, src, static_cast<size_t>(size));
  lRasterBufSize = size;
  fLocRect = loc;
  lImageCode = code;
  return k_raster_ok;
}

long OgrRasterLayer::get_raster_no_clone(char*& pRasterBuf, long& lRasterBufSize,
                                      Envelope& fLocRect,
                                      long& lImageCode) const {
  fLocRect = rect_;
  lImageCode = image_code_;
  if (vsimem_blob_.empty()) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return k_raster_ok;
  }
  vsi_l_offset length = 0;
  GByte* data = VSIGetMemFileBuffer(vsimem_blob_.c_str(), &length, FALSE);
  if (!data) {
    pRasterBuf = nullptr;
    lRasterBufSize = 0;
    return k_raster_fail;
  }
  pRasterBuf = reinterpret_cast<char*>(data);
  lRasterBufSize = static_cast<long>(length);
  return k_raster_ok;
}

long OgrRasterLayer::get_raster_rect(Envelope& fLocRect) const {
  fLocRect = rect_;
  return k_raster_ok;
}

}  // namespace datasource
}  // namespace gis
