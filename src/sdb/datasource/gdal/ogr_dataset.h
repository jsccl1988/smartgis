// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_DATASET_H_
#define SDB_DATASOURCE_GDAL_OGR_DATASET_H_

#include "layer.h"
#include "sdb/datasource/gdal/ogr_export.h"

class GDALDataset;

namespace sdb {
namespace datasource {

// OGR/GDAL-backed SmtDataSource for GPKG, SpatiaLite, and PostGIS.
class SMT_SDE_GDAL_EXPORT OgrDataSource : public Smt_GIS::SmtDataSource {
 public:
  OgrDataSource();
  ~OgrDataSource() override;

  bool Create() override;
  bool Open() override;
  bool Close() override;
  Smt_GIS::SmtDataSource* Clone() const override;

  Smt_GIS::SmtVectorLayer* CreateVectorLayer(
      const char* szName, Smt_Core::fRect& lyrRect,
      Smt_GIS::SmtFeatureType ftType = Smt_GIS::SmtFtDot) override;
  Smt_GIS::SmtVectorLayer* OpenVectorLayer(const char* szName) override;
  bool DeleteVectorLayer(const char* szName) override;

  Smt_GIS::SmtRasterLayer* CreateRasterLayer(const char* szName,
                                             Smt_Core::fRect& lyrRect,
                                             long lImageCode) override;
  Smt_GIS::SmtRasterLayer* OpenRasterLayer(const char* szName) override;
  bool DeleteRasterLayer(const char* szName) override;

  Smt_GIS::SmtTileLayer* CreateTileLayer(const char* szName,
                                         Smt_Core::fRect& lyrRect,
                                         long lImageCode) override;
  Smt_GIS::SmtTileLayer* OpenTileLayer(const char* szName) override;
  bool DeleteTileLayer(const char* szName) override;

  GDALDataset* dataset() { return dataset_; }

 private:
  void fill_layer_infos();

  GDALDataset* dataset_;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_DATASET_H_
