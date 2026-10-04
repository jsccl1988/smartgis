// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_LAYER_RASTER_WRAP_H_
#define SMT_LEGACY_GIS_LAYER_RASTER_WRAP_H_

#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "legacy/gis/layer/layer.h"

namespace gis {

inline Envelope leftover_env_from_rect(const fRect& r) {
  Envelope e;
  e.MinX = r.lb.x;
  e.MinY = r.lb.y;
  e.MaxX = r.rt.x;
  e.MaxY = r.rt.y;
  return e;
}

inline fRect leftover_rect_from_env(const Envelope& e) {
  fRect r;
  r.lb.x = static_cast<float>(e.MinX);
  r.lb.y = static_cast<float>(e.MinY);
  r.rt.x = static_cast<float>(e.MaxX);
  r.rt.y = static_cast<float>(e.MaxY);
  return r;
}

// Leftover RasterLayer ABI over product OgrRasterLayer.
class LeftoverOgrRasterLayer : public RasterLayer {
 public:
  LeftoverOgrRasterLayer(datasource::OgrRasterLayer* inner, bool owns)
      : inner_(inner), owns_(owns && inner != nullptr) {
    if (inner_) {
      SetLayerName(inner_->GetLayerName());
      m_pOwnerDs = inner_->dataset();
      m_bOpen = inner_->IsOpen();
      inner_->get_envelope(m_lyrEnv);
    }
  }

  ~LeftoverOgrRasterLayer() override {
    if (owns_) {
      delete inner_;
    }
    inner_ = nullptr;
  }

  datasource::OgrRasterLayer* inner() { return inner_; }
  const datasource::OgrRasterLayer* inner() const { return inner_; }
  datasource::OgrRasterLayer* release_inner() {
    owns_ = false;
    datasource::OgrRasterLayer* p = inner_;
    inner_ = nullptr;
    return p;
  }

  bool Create() override {
    if (!inner_ || !inner_->Create()) {
      return false;
    }
    m_bOpen = true;
    m_pOwnerDs = inner_->dataset();
    inner_->CalEnvelope();
    inner_->get_envelope(m_lyrEnv);
    return true;
  }

  bool Open(const char* szLayerArchiveName) override {
    if (!inner_ || !inner_->Open(szLayerArchiveName)) {
      return false;
    }
    SetLayerName(inner_->GetLayerName());
    m_bOpen = true;
    m_pOwnerDs = inner_->dataset();
    inner_->get_envelope(m_lyrEnv);
    return true;
  }

  bool Close() override {
    if (!inner_) {
      return false;
    }
    const bool ok = inner_->Close();
    m_bOpen = false;
    m_pOwnerDs = nullptr;
    return ok;
  }

  bool Fetch(FetchType type = FETCH_ALL) override {
    (void)type;
    return inner_ && inner_->Fetch();
  }

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const fRect& fLocRect, long lImageCode) override {
    if (!inner_) {
      return SMT_ERR_FAILURE;
    }
    const long rc = inner_->CreaterRaster(pRasterBuf, lRasterBufSize,
                                          leftover_env_from_rect(fLocRect),
                                          lImageCode);
    return rc == datasource::k_raster_ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

  long SetRasterRect(const fRect& fLocRect) override {
    if (!inner_) {
      return SMT_ERR_FAILURE;
    }
    const long rc = inner_->SetRasterRect(leftover_env_from_rect(fLocRect));
    inner_->get_envelope(m_lyrEnv);
    return rc == datasource::k_raster_ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

  long GetRaster(char*& pRasterBuf, long& lRasterBufSize, fRect& fLocRect,
                 long& lImageCode) const override {
    if (!inner_) {
      return SMT_ERR_FAILURE;
    }
    Envelope loc;
    const long rc =
        inner_->GetRaster(pRasterBuf, lRasterBufSize, loc, lImageCode);
    fLocRect = leftover_rect_from_env(loc);
    return rc == datasource::k_raster_ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                        fRect& fLocRect, long& lImageCode) const override {
    if (!inner_) {
      return SMT_ERR_FAILURE;
    }
    Envelope loc;
    const long rc =
        inner_->GetRasterNoClone(pRasterBuf, lRasterBufSize, loc, lImageCode);
    fLocRect = leftover_rect_from_env(loc);
    return rc == datasource::k_raster_ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

  long GetRasterRect(fRect& fLocRect) const override {
    if (!inner_) {
      return SMT_ERR_FAILURE;
    }
    Envelope loc;
    const long rc = inner_->GetRasterRect(loc);
    fLocRect = leftover_rect_from_env(loc);
    return rc == datasource::k_raster_ok ? SMT_ERR_NONE : SMT_ERR_FAILURE;
  }

 private:
  datasource::OgrRasterLayer* inner_ = nullptr;
  bool owns_ = false;
};

}  // namespace gis

#endif  // SMT_LEGACY_GIS_LAYER_RASTER_WRAP_H_
