// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_DATASET_H_
#define SDB_DATASOURCE_GDAL_OGR_DATASET_H_

#include "layer.h"
#include "sdb/datasource/gdal/ogr_export.h"

class GDALDataset;
class OGRLayer;

namespace sdb {
namespace datasource {

// Opens or creates a GDALDataset from SmtDataSourceInfo. Not a SmtDataSource.
class SMT_SDE_GDAL_EXPORT OgrDataSource {
 public:
  OgrDataSource();
  ~OgrDataSource();

  OgrDataSource(const OgrDataSource&) = delete;
  OgrDataSource& operator=(const OgrDataSource&) = delete;

  bool Create();
  bool Open();
  bool Close();
  bool IsOpen() const { return open_; }

  void SetInfo(const Smt_GIS::SmtDataSourceInfo& info) { info_ = info; }
  void GetInfo(Smt_GIS::SmtDataSourceInfo& info) const { info = info_; }

  GDALDataset* dataset() { return dataset_; }
  GDALDataset* release();

  OGRLayer* CreateVectorLayer(const char* szName, Smt_Core::fRect& lyrRect,
                              Smt_GIS::SmtFeatureType ftType);
  OGRLayer* OpenVectorLayer(const char* szName);
  bool DeleteVectorLayer(const char* szName);

  Smt_GIS::SmtRasterLayer* CreateRasterLayer(const char* szName,
                                             Smt_Core::fRect& lyrRect,
                                             long lImageCode);
  Smt_GIS::SmtRasterLayer* OpenRasterLayer(const char* szName);

 private:
  Smt_GIS::SmtDataSourceInfo info_;
  GDALDataset* dataset_ = nullptr;
  bool open_ = false;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_DATASET_H_
