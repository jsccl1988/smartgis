// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/model/feature/feature_api.h"

#include "ogrsf_frmts.h"

long append_cloned_feature(OGRLayer* dest, const OGRFeature* src) {
  if (!dest || !src) {
    return SMT_ERR_INVALID_PARAM;
  }
  OGRFeature* out = OGRFeature::CreateFeature(dest->GetLayerDefn());
  if (!out) {
    return SMT_ERR_FAILURE;
  }
  out->SetFrom(src);
  if (src->GetGeometryRef()) {
    out->SetGeometry(src->GetGeometryRef());
  }
  const OGRErr err = dest->CreateFeature(out);
  OGRFeature::DestroyFeature(out);
  return err == OGRERR_NONE ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

long copy_layer(OGRLayer* pTarLayer, OGRLayer* pSrcLayer) {
  if (!pTarLayer || !pSrcLayer) {
    return SMT_ERR_INVALID_PARAM;
  }
  pSrcLayer->ResetReading();
  while (OGRFeature* feat = pSrcLayer->GetNextFeature()) {
    append_cloned_feature(pTarLayer, feat);
    OGRFeature::DestroyFeature(feat);
  }
  return SMT_ERR_NONE;
}

long copy_layer(SmtLayer* pTarLayer, SmtLayer* pSrcLayer, bool bClone,
                bool bCheckFeaType) {
  if (!pTarLayer || !pSrcLayer ||
      pSrcLayer->GetLayerType() != pTarLayer->GetLayerType()) {
    return SMT_ERR_INVALID_PARAM;
  }
  if (pSrcLayer->GetLayerType() == LYR_RASTER) {
    return copy_layer(static_cast<SmtRasterLayer*>(pTarLayer),
                      static_cast<SmtRasterLayer*>(pSrcLayer), bClone,
                      bCheckFeaType);
  }
  return SMT_ERR_FAILURE;
}

long copy_layer(SmtRasterLayer* pTarLayer, SmtRasterLayer* pSrcLayer,
                bool /*bClone*/, bool /*bCheckFeaType*/) {
  if (!pTarLayer || !pSrcLayer) {
    return SMT_ERR_INVALID_PARAM;
  }
  char* pRasterBuf = nullptr;
  long lRasterBufSize = 0;
  long lCodeType = -1;
  fRect locRect;
  if (SMT_ERR_NONE == pSrcLayer->GetRasterNoClone(pRasterBuf, lRasterBufSize,
                                                  locRect, lCodeType) &&
      SMT_ERR_NONE == pTarLayer->CreaterRaster(pRasterBuf, lRasterBufSize,
                                               locRect, lCodeType)) {
    pTarLayer->CalEnvelope();
    return SMT_ERR_NONE;
  }
  return SMT_ERR_FAILURE;
}
