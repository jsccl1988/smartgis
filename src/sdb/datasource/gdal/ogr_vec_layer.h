// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_VEC_LAYER_H_
#define SDB_DATASOURCE_GDAL_OGR_VEC_LAYER_H_

#include "layer.h"
#include "sdb/datasource/gdal/ogr_export.h"

#include <vector>

class OGRLayer;

namespace sdb {
namespace datasource {

class OgrDataSource;

// Single vector layer class for all SmtFeatureType kinds (traits pick WKB).
class SMT_SDE_GDAL_EXPORT OgrVectorLayer : public Smt_GIS::SmtVectorLayer {
 public:
  explicit OgrVectorLayer(OgrDataSource* owner);
  ~OgrVectorLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(Smt_GIS::eSmtFetchType type = Smt_GIS::FETCH_ALL) override;

  int GetFeatureCount() const override;
  void MoveFirst() const override;
  void MoveNext() const override;
  void MoveLast() const override;
  void Delete() override;
  bool IsEnd() const override;
  void DeleteAll() override;

  long CreateSpatialIndex(const char* szName, uint type) override;
  long Query(const Smt_GIS::SmtGQueryDesc* pGQueryDesc,
             const Smt_GIS::SmtPQueryDesc* pPQueryDesc,
             Smt_GIS::SmtVectorLayer* pQueryResult) override;

  long AppendFeature(const Smt_GIS::SmtFeature* pSmtFeature,
                     bool bClone = false) override;
  long AppendFeatureBatch(const Smt_GIS::SmtFeature* pSmtFeature,
                          bool bClone = false) override;
  long UpdateFeatureBatch() override;
  long UpdateFeature(const Smt_GIS::SmtFeature* pSmtFeature) override;
  long DeleteFeature(const Smt_GIS::SmtFeature* pSmtFeature) override;

  Smt_GIS::SmtFeature* GetFeature() const override;
  Smt_GIS::SmtFeature* GetFeature(int index) const override;
  Smt_GIS::SmtFeature* GetFeatureByID(uint unID) const override;

  long StartTransaction() override;
  long CommitTransaction() override;
  long RollbackTransaction() override;

  void CalEnvelope() override;

 private:
  OgrDataSource* owner_;
  OGRLayer* layer_;
  std::vector<Smt_GIS::SmtFeature*> features_;
  mutable int iterator_;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_VEC_LAYER_H_
