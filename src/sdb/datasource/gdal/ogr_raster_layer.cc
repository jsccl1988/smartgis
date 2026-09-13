// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_raster_layer.h"

#include "sdb/datasource/gdal/ogr_dataset.h"

#include "gdal_priv.h"

namespace sdb {
namespace datasource {

OgrRasterLayer::OgrRasterLayer(OgrDataSource* owner)
    : Smt_GIS::SmtRasterLayer(owner), owner_(owner) {
  rect_.lb.x = 0;
  rect_.lb.y = 0;
  rect_.rt.x = 0;
  rect_.rt.y = 0;
}

OgrRasterLayer::~OgrRasterLayer() = default;

bool OgrRasterLayer::Create() {
  if (owner_ && owner_->dataset() && owner_->dataset()->GetRasterCount() > 0) {
    m_bOpen = true;
    return true;
  }
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

bool OgrRasterLayer::Fetch(Smt_GIS::eSmtFetchType /*type*/) {
  return IsOpen();
}

long OgrRasterLayer::CreaterRaster(const char* /*pRasterBuf*/,
                                   long /*lRasterBufSize*/,
                                   const Smt_Core::fRect& /*fLocRect*/,
                                   long /*lImageCode*/) {
  return SMT_ERR_UNSUPPORTED;
}

long OgrRasterLayer::SetRasterRect(const Smt_Core::fRect& fLocRect) {
  rect_ = fLocRect;
  return SMT_ERR_NONE;
}

long OgrRasterLayer::GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                               Smt_Core::fRect& fLocRect,
                               long& lImageCode) const {
  pRasterBuf = nullptr;
  lRasterBufSize = 0;
  fLocRect = rect_;
  lImageCode = 0;
  return SMT_ERR_UNSUPPORTED;
}

long OgrRasterLayer::GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                                      Smt_Core::fRect& fLocRect,
                                      long& lImageCode) const {
  return GetRaster(pRasterBuf, lRasterBufSize, fLocRect, lImageCode);
}

long OgrRasterLayer::GetRasterRect(Smt_Core::fRect& fLocRect) const {
  fLocRect = rect_;
  return SMT_ERR_NONE;
}

}  // namespace datasource
}  // namespace sdb
