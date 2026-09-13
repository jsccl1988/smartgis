// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_DATASET_H_
#define SDB_DATASOURCE_GDAL_SDBD_DATASET_H_

#include "sdb/feature/feature.h"
#include "sdb/layer/layer.h"
#include "sdb/datasource/gdal/ogr_export.h"
#include "sdb/datasource/gdal/sdbd_gdal_driver.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <memory>
#include <vector>

namespace sdb {
namespace datasource {

class SdbdLayer;

// GDALDataset subclass returned by driver "SDBD". Owns a stock inner dataset
// (Memory / GPKG / PostgreSQL / file). GetLayer / CreateLayer return SdbdLayer.
class SDE_GDAL_EXPORT SdbdDataset final : public GDALDataset {
 public:
  explicit SdbdDataset(GDALDataset* inner);
  ~SdbdDataset() override;

  SdbdDataset(const SdbdDataset&) = delete;
  SdbdDataset& operator=(const SdbdDataset&) = delete;

  GDALDataset* inner() { return inner_; }
  const GDALDataset* inner() const { return inner_; }

  SdbdLayer* sdbd_layer(int index);
  SdbdLayer* sdbd_layer_by_name(const char* name);
  SdbdLayer* create_sdbd_layer(const char* name, sdb::SmtFeatureType ft);

  int GetLayerCount() override;
  OGRLayer* GetLayer(int i) override;
  OGRLayer* GetLayerByName(const char* name) override;
  OGRErr DeleteLayer(int i) override;
  int TestCapability(const char* cap) override;

  CPLErr FlushCache(bool at_closing = false) override;
  const OGRSpatialReference* GetSpatialRef() const override;
  CPLErr SetSpatialRef(const OGRSpatialReference* srs) override;
  CPLErr GetGeoTransform(double* transform) override;
  CPLErr SetGeoTransform(double* transform) override;
  CPLErr AddBand(GDALDataType type, char** options = nullptr) override;
  GDALDriver* GetDriver() override;
  char** GetFileList() override;
  const OGRSpatialReference* GetGCPSpatialRef() const override;
  int GetGCPCount() override;
  const GDAL_GCP* GetGCPs() override;
  CPLErr SetGCPs(int count, const GDAL_GCP* gcps,
                 const OGRSpatialReference* srs) override;

  static int identify(GDALOpenInfo* info);
  static GDALDataset* open(GDALOpenInfo* info);
  static GDALDataset* create(const char* name, int x, int y, int bands,
                             GDALDataType type, char** opts);

 protected:
  OGRLayer* ICreateLayer(const char* name, const OGRSpatialReference* srs,
                         OGRwkbGeometryType gtype, char** options) override;
  CPLErr IRasterIO(GDALRWFlag rw, int x_off, int y_off, int x_size, int y_size,
                   void* data, int buf_x, int buf_y, GDALDataType type,
                   int band_count, int* band_map, GSpacing pixel_space,
                   GSpacing line_space, GSpacing band_space,
                   GDALRasterIOExtraArg* extra) override;

 private:
  SdbdLayer* wrap(OGRLayer* inner);
  void drop_wrappers();

  GDALDataset* inner_ = nullptr;
  std::vector<std::unique_ptr<SdbdLayer>> wrappers_;
};

// GDALOpenEx / Create via "SDBD". Empty target (ACCESS, WS, …) returns null.
SDE_GDAL_EXPORT GDALDataset* open_sdbd_dataset(
    const sdb::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_SDBD_DATASET_H_
