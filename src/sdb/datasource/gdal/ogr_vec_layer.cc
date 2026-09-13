// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_vec_layer.h"

#include "sdb/datasource/gdal/ogr_dataset.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "sdb/datasource/gdal/ogr_feature_kind.h"

#include "envelope.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <string>

namespace sdb {
namespace datasource {

OgrVectorLayer::OgrVectorLayer(OgrDataSource* owner)
    : Smt_GIS::SmtVectorLayer(owner),
      owner_(owner),
      layer_(nullptr),
      iterator_(0) {}

OgrVectorLayer::~OgrVectorLayer() {
  if (IsOpen()) {
    Close();
  }
  DeleteAll();
}

bool OgrVectorLayer::Create() {
  if (!owner_ || !owner_->dataset()) {
    return false;
  }
  GDALDataset* ds = owner_->dataset();
  const bool ok = visit_feature_kind(m_SmtLayerFtType, [&](auto traits) {
    using Traits = decltype(traits);
    if (Traits::is_raster) {
      return false;
    }
    layer_ = ds->CreateLayer(m_szLayerName, nullptr, Traits::wkb, nullptr);
    if (!layer_) {
      return false;
    }
    OGRFieldDefn style("style", OFTBinary);
    layer_->CreateField(&style);
    for_each_extra_field<typename Traits::extra_fields>([&](auto field) {
      OGRFieldDefn defn(field.name, field.ogr_type);
      layer_->CreateField(&defn);
    });
    return true;
  });
  m_bOpen = ok && layer_ != nullptr;
  return m_bOpen;
}

bool OgrVectorLayer::Open(const char* szLayerArchiveName) {
  if (!owner_ || !owner_->dataset() || !szLayerArchiveName) {
    return false;
  }
  layer_ = owner_->dataset()->GetLayerByName(szLayerArchiveName);
  m_bOpen = layer_ != nullptr;
  if (m_bOpen) {
    SetLayerName(szLayerArchiveName);
  }
  return m_bOpen;
}

bool OgrVectorLayer::Close() {
  if (layer_) {
    layer_->SyncToDisk();
  }
  layer_ = nullptr;
  m_bOpen = false;
  return true;
}

bool OgrVectorLayer::Fetch(Smt_GIS::eSmtFetchType /*type*/) {
  if (!IsOpen() || !layer_) {
    return false;
  }
  DeleteAll();
  layer_->ResetReading();
  while (OGRFeature* ogr = layer_->GetNextFeature()) {
    auto* smt = new Smt_GIS::SmtFeature();
    if (copy_ogr_feature_to_smt(ogr, smt, m_SmtLayerFtType)) {
      features_.push_back(smt);
    } else {
      delete smt;
    }
    OGRFeature::DestroyFeature(ogr);
  }
  iterator_ = 0;
  return true;
}

int OgrVectorLayer::GetFeatureCount() const {
  return static_cast<int>(features_.size());
}

void OgrVectorLayer::MoveFirst() const {
  iterator_ = 0;
}

void OgrVectorLayer::MoveNext() const {
  if (iterator_ < static_cast<int>(features_.size())) {
    ++iterator_;
  }
}

void OgrVectorLayer::MoveLast() const {
  iterator_ = features_.empty() ? 0 : static_cast<int>(features_.size()) - 1;
}

void OgrVectorLayer::Delete() {
  if (iterator_ < 0 || iterator_ >= static_cast<int>(features_.size())) {
    return;
  }
  DeleteFeature(features_[iterator_]);
}

bool OgrVectorLayer::IsEnd() const {
  return iterator_ >= static_cast<int>(features_.size());
}

void OgrVectorLayer::DeleteAll() {
  for (Smt_GIS::SmtFeature* f : features_) {
    delete f;
  }
  features_.clear();
  iterator_ = 0;
}

long OgrVectorLayer::CreateSpatialIndex(const char* /*szName*/, uint /*type*/) {
  if (!IsOpen() || !layer_ || !owner_ || !owner_->dataset()) {
    return SMT_ERR_DB_OPER;
  }
  const char* name = layer_->GetName();
  if (!name || name[0] == '\0') {
    return SMT_ERR_UNSUPPORTED;
  }
  std::string sql = "CREATE SPATIAL INDEX ON ";
  sql += name;
  CPLErrorReset();
  OGRLayer* result = owner_->dataset()->ExecuteSQL(sql.c_str(), nullptr, nullptr);
  if (result) {
    owner_->dataset()->ReleaseResultSet(result);
  }
  if (CPLGetLastErrorType() == CE_None || CPLGetLastErrorType() == CE_Debug) {
    return SMT_ERR_NONE;
  }
  return SMT_ERR_UNSUPPORTED;
}

long OgrVectorLayer::Query(const Smt_GIS::SmtGQueryDesc* pGQueryDesc,
                           const Smt_GIS::SmtPQueryDesc* pPQueryDesc,
                           Smt_GIS::SmtVectorLayer* pQueryResult) {
  if (!IsOpen() || !layer_) {
    return SMT_ERR_DB_OPER;
  }
  if (!pQueryResult) {
    return SMT_ERR_UNSUPPORTED;
  }

  if (pGQueryDesc && pGQueryDesc->pQueryGeom) {
    Smt_Base::Envelope env;
    pGQueryDesc->pQueryGeom->GetEnvelope(&env);
    layer_->SetSpatialFilterRect(env.MinX, env.MinY, env.MaxX, env.MaxY);
  }

  if (pPQueryDesc && pPQueryDesc->szFldName && pPQueryDesc->szFldName[0] &&
      pPQueryDesc->szFldQueryContent && pPQueryDesc->szFldQueryContent[0]) {
    std::string attr = pPQueryDesc->szFldName[0];
    attr += pPQueryDesc->szFldQueryContent[0];
    if (layer_->SetAttributeFilter(attr.c_str()) != OGRERR_NONE) {
      layer_->SetAttributeFilter(nullptr);
    }
  }

  layer_->ResetReading();
  while (OGRFeature* ogr = layer_->GetNextFeature()) {
    auto* smt = new Smt_GIS::SmtFeature();
    bool keep = copy_ogr_feature_to_smt(ogr, smt, m_SmtLayerFtType);
    if (keep && pGQueryDesc && pGQueryDesc->pQueryGeom && smt->GetGeometryRef()) {
      const long rs = pGQueryDesc->pQueryGeom->Relationship(
          smt->GetGeometryRef(), pGQueryDesc->fSmargin);
      keep = (pGQueryDesc->sSRs & rs) != 0;
    }
    if (keep) {
      pQueryResult->AppendFeature(smt, true);
    }
    delete smt;
    OGRFeature::DestroyFeature(ogr);
  }

  layer_->SetSpatialFilter(nullptr);
  layer_->SetAttributeFilter(nullptr);
  return SMT_ERR_NONE;
}

long OgrVectorLayer::AppendFeature(const Smt_GIS::SmtFeature* pSmtFeature,
                                   bool /*bClone*/) {
  if (!IsOpen() || !layer_ || !pSmtFeature) {
    return SMT_ERR_DB_OPER;
  }
  OGRFeature ogr(layer_->GetLayerDefn());
  if (!copy_smt_feature_to_ogr(pSmtFeature, &ogr)) {
    return SMT_ERR_FAILURE;
  }
  if (layer_->CreateFeature(&ogr) != OGRERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  Smt_GIS::SmtFeature* clone = pSmtFeature->Clone();
  clone->SetID(static_cast<long>(ogr.GetFID()));
  features_.push_back(clone);
  return SMT_ERR_NONE;
}

long OgrVectorLayer::AppendFeatureBatch(const Smt_GIS::SmtFeature* pSmtFeature,
                                        bool bClone) {
  return AppendFeature(pSmtFeature, bClone);
}

long OgrVectorLayer::UpdateFeatureBatch() {
  return SMT_ERR_NONE;
}

long OgrVectorLayer::UpdateFeature(const Smt_GIS::SmtFeature* pSmtFeature) {
  if (!IsOpen() || !layer_ || !pSmtFeature) {
    return SMT_ERR_DB_OPER;
  }
  OGRFeature ogr(layer_->GetLayerDefn());
  if (!copy_smt_feature_to_ogr(pSmtFeature, &ogr)) {
    return SMT_ERR_FAILURE;
  }
  if (layer_->SetFeature(&ogr) != OGRERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  for (size_t i = 0; i < features_.size(); ++i) {
    if (features_[i] && features_[i]->GetID() == pSmtFeature->GetID()) {
      delete features_[i];
      features_[i] = pSmtFeature->Clone();
      return SMT_ERR_NONE;
    }
  }
  features_.push_back(pSmtFeature->Clone());
  return SMT_ERR_NONE;
}

long OgrVectorLayer::DeleteFeature(const Smt_GIS::SmtFeature* pSmtFeature) {
  if (!IsOpen() || !layer_ || !pSmtFeature) {
    return SMT_ERR_DB_OPER;
  }
  if (layer_->DeleteFeature(pSmtFeature->GetID()) != OGRERR_NONE) {
    return SMT_ERR_FAILURE;
  }
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    if (*it && (*it)->GetID() == pSmtFeature->GetID()) {
      delete *it;
      features_.erase(it);
      return SMT_ERR_NONE;
    }
  }
  return SMT_ERR_FAILURE;
}

Smt_GIS::SmtFeature* OgrVectorLayer::GetFeature() const {
  return GetFeature(iterator_);
}

Smt_GIS::SmtFeature* OgrVectorLayer::GetFeature(int index) const {
  if (index < 0 || index >= static_cast<int>(features_.size())) {
    return nullptr;
  }
  return features_[index];
}

Smt_GIS::SmtFeature* OgrVectorLayer::GetFeatureByID(uint unID) const {
  for (Smt_GIS::SmtFeature* f : features_) {
    if (f && static_cast<uint>(f->GetID()) == unID) {
      return f;
    }
  }
  return nullptr;
}

long OgrVectorLayer::StartTransaction() {
  if (!owner_ || !owner_->dataset()) {
    return SMT_ERR_NONE;
  }
  if (owner_->dataset()->TestCapability(ODsCTransactions)) {
    return owner_->dataset()->StartTransaction() == OGRERR_NONE
               ? SMT_ERR_NONE
               : SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

long OgrVectorLayer::CommitTransaction() {
  if (!owner_ || !owner_->dataset()) {
    return SMT_ERR_NONE;
  }
  if (owner_->dataset()->TestCapability(ODsCTransactions)) {
    return owner_->dataset()->CommitTransaction() == OGRERR_NONE
               ? SMT_ERR_NONE
               : SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

long OgrVectorLayer::RollbackTransaction() {
  if (!owner_ || !owner_->dataset()) {
    return SMT_ERR_NONE;
  }
  if (owner_->dataset()->TestCapability(ODsCTransactions)) {
    return owner_->dataset()->RollbackTransaction() == OGRERR_NONE
               ? SMT_ERR_NONE
               : SMT_ERR_FAILURE;
  }
  return SMT_ERR_NONE;
}

void OgrVectorLayer::CalEnvelope() {
  m_lyrEnv = Smt_Base::Envelope();
  for (Smt_GIS::SmtFeature* f : features_) {
    if (!f || !f->GetGeometryRef()) {
      continue;
    }
    Smt_Base::Envelope env;
    f->GetGeometryRef()->GetEnvelope(&env);
    m_lyrEnv.Merge(env);
  }
}

}  // namespace datasource
}  // namespace sdb
