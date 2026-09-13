// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/feature/feature.h"

#include "base/core/core.h"
#include "base/style/stylemanager.h"

#include "ogrsf_frmts.h"

using namespace base;
using namespace base;

namespace sdb {

SmtFeature::SmtFeature() = default;

SmtFeature::~SmtFeature() {
  SMT_SAFE_DELETE(att_);
  SMT_SAFE_DELETE(geom_);
}

void SmtFeature::SetFeatureType(SmtFeatureType type) {
  if (type_ == type) {
    return;
  }
  type_ = type;
  SMT_SAFE_DELETE(att_);
  att_ = new SmtAttribute();
  SmtField fld;
  switch (type_) {
    case SmtFtAnno:
      fld.SetName("anno");
      fld.SetType(SmtVarType::SmtString);
      att_->AddField(fld);
      fld.SetName("color");
      fld.SetType(SmtVarType::SmtInteger);
      att_->AddField(fld);
      fld.SetName("angle");
      fld.SetType(SmtVarType::SmtReal);
      att_->AddField(fld);
      break;
    case SmtFtCurve:
      fld.SetName("length");
      fld.SetType(SmtVarType::SmtReal);
      att_->AddField(fld);
      break;
    case SmtFtSurface:
      fld.SetName("area");
      fld.SetType(SmtVarType::SmtReal);
      att_->AddField(fld);
      break;
    default:
      break;
  }
}

void SmtFeature::SetGeometryDirectly(OGRGeometry* geom) {
  SMT_SAFE_DELETE(geom_);
  geom_ = geom;
}

void SmtFeature::SetGeometry(OGRGeometry* geom) {
  SMT_SAFE_DELETE(geom_);
  geom_ = geom ? geom->clone() : nullptr;
}

void SmtFeature::SetGeometry(geo::Grid* grid) { grid_ = grid; }

void SmtFeature::SetGeometry(geo::Tin* tin) { tin_ = tin; }

void SmtFeature::SetStyle(SmtStyle* style) { style_ = style; }

void SmtFeature::SetStyle(const char* style_name) {
  SmtStyleManager* mgr = SmtStyleManager::get_singleton_ptr();
  style_ = (mgr && style_name) ? mgr->get_style(style_name) : nullptr;
}

int SmtFeature::GetFieldIndexByName(const char* name) {
  return att_ ? att_->GetFieldIndex(name) : -1;
}

int SmtFeature::SetFieldValue(int index, int nValue) {
  if (!att_) {
    return SMT_ERR_FAILURE;
  }
  SmtField* field = att_->GetFieldPtr(index);
  return field ? field->SetValue(nValue) : SMT_ERR_FAILURE;
}

int SmtFeature::SetFieldValue(int index, double dfValue) {
  if (!att_) {
    return SMT_ERR_FAILURE;
  }
  SmtField* field = att_->GetFieldPtr(index);
  return field ? field->SetValue(dfValue) : SMT_ERR_FAILURE;
}

int SmtFeature::SetFieldValue(int index, const char* pszValue) {
  if (!att_) {
    return SMT_ERR_FAILURE;
  }
  SmtField* field = att_->GetFieldPtr(index);
  return field ? field->SetValue(pszValue) : SMT_ERR_FAILURE;
}

bool leftover_append_feature(OGRLayer* layer, SmtFeature* feature) {
  if (!layer || !feature) {
    return false;
  }
  OGRFeature* dst = OGRFeature::CreateFeature(layer->GetLayerDefn());
  if (!dst) {
    return false;
  }
  if (feature->GetGeometryRef()) {
    dst->SetGeometry(feature->GetGeometryRef());
  }
  dst->SetFID(feature->GetID());
  const OGRErr err = layer->CreateFeature(dst);
  OGRFeature::DestroyFeature(dst);
  return err == OGRERR_NONE;
}

}  // namespace sdb
