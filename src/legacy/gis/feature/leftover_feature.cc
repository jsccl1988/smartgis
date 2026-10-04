// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/feature/leftover_feature.h"

#include "ogrsf_frmts.h"

namespace gis {

bool leftover_append_feature(OGRLayer* layer, Feature* feature) {
  if (!layer || !feature) {
    return false;
  }
  OGRFeature* src = feature->ogr();
  if (src && src->GetDefnRef() == layer->GetLayerDefn()) {
    return layer->CreateFeature(src) == OGRERR_NONE;
  }
  OGRFeature* dst = OGRFeature::CreateFeature(layer->GetLayerDefn());
  if (!dst) {
    return false;
  }
  if (src) {
    dst->SetFrom(src);
  } else if (feature->geometry()) {
    dst->SetGeometry(feature->geometry());
  }
  dst->SetFID(feature->id());
  const OGRErr err = layer->CreateFeature(dst);
  OGRFeature::DestroyFeature(dst);
  return err == OGRERR_NONE;
}

}  // namespace gis
