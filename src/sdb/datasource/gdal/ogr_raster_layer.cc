// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_raster_layer.h"

#include "gdal_priv.h"

namespace sdb {
namespace datasource {

OgrRasterLayer::OgrRasterLayer(GDALDataset* owner)
    : sdb::SmtRasterLayer(owner) {
  rect_.lb.x = 0;
  rect_.lb.y = 0;
  rect_.rt.x = 0;
  rect_.rt.y = 0;
}

OgrRasterLayer::~OgrRasterLayer() = default;

bool OgrRasterLayer::Create() {
  // Band I/O is UNSUPPORTED; do not claim Create success. Listing rasters
  // happens on the dataset. Do not invent a geom_points blob table.
  m_bOpen = false;
  return false;
}

bool OgrRasterLayer::Open(const char* /*szLayerArchiveName*/) {
  return Create();
}

bool OgrRasterLayer::Close() {
  m_bOpen = false;
  return true;
}

bool OgrRasterLayer::Fetch(sdb::eSmtFetchType /*type*/) {
  return IsOpen();
}

long OgrRasterLayer::CreaterRaster(const char* /*pRasterBuf*/,
                                   long /*lRasterBufSize*/,
                                   const base::fRect& /*fLocRect*/,
                                   long /*lImageCode*/) {
  return SMT_ERR_UNSUPPORTED;
}

long OgrRasterLayer::SetRasterRect(const base::fRect& fLocRect) {
  rect_ = fLocRect;
  return SMT_ERR_NONE;
}

long OgrRasterLayer::GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                               base::fRect& fLocRect,
                               long& lImageCode) const {
  pRasterBuf = nullptr;
  lRasterBufSize = 0;
  fLocRect = rect_;
  lImageCode = 0;
  return SMT_ERR_UNSUPPORTED;
}

long OgrRasterLayer::GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                                      base::fRect& fLocRect,
                                      long& lImageCode) const {
  return GetRaster(pRasterBuf, lRasterBufSize, fLocRect, lImageCode);
}

long OgrRasterLayer::GetRasterRect(base::fRect& fLocRect) const {
  fLocRect = rect_;
  return SMT_ERR_NONE;
}

}  // namespace datasource
}  // namespace sdb
