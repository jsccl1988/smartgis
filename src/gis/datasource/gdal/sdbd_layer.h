// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_LAYER_H_
#define SDB_DATASOURCE_GDAL_SDBD_LAYER_H_

#include <string>

#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/ogr_export.h"
#include "gis/feature/feature.h"

namespace gis {
namespace datasource {

class SdbdDataset;

// OGRLayer subclass returned by SdbdDataset. Forwards to a stock inner layer
// and holds product extras (feature kind, style hint, catalog name).
class SDE_GDAL_EXPORT SdbdLayer final : public OGRLayer {
 public:
  SdbdLayer(OGRLayer* inner, gis::SmtFeatureType ft,
            SdbdDataset* owner = nullptr);
  ~SdbdLayer() override = default;

  SdbdLayer(const SdbdLayer&) = delete;
  SdbdLayer& operator=(const SdbdLayer&) = delete;

  OGRLayer* inner() { return inner_; }
  const OGRLayer* inner() const { return inner_; }

  gis::SmtFeatureType feature_type() const { return feature_type_; }
  void set_feature_type(gis::SmtFeatureType ft);

  const char* style_hint() const { return style_hint_.c_str(); }
  void set_style_hint(const char* name);

  const char* GetName() override;
  OGRwkbGeometryType GetGeomType() override;
  OGRFeatureDefn* GetLayerDefn() override;
  void ResetReading() override;
  OGRFeature* GetNextFeature() override;
  OGRFeature* GetFeature(GIntBig fid) override;
  GIntBig GetFeatureCount(int force = TRUE) override;
  OGRErr SetNextByIndex(GIntBig index) override;
  OGRErr ISetFeature(OGRFeature* feature) override;
  OGRErr ICreateFeature(OGRFeature* feature) override;
  OGRErr DeleteFeature(GIntBig fid) override;
  OGRErr CreateField(const OGRFieldDefn* defn, int approx = TRUE) override;
  OGRErr DeleteField(int index) override;
  OGRErr ReorderFields(int* map) override;
  OGRErr AlterFieldDefn(int index, OGRFieldDefn* defn, int flags) override;
  OGRSpatialReference* GetSpatialRef() override;
  OGRErr GetExtent(OGREnvelope* env, int force = TRUE) override;
  OGRErr GetExtent(int geom, OGREnvelope* env, int force = TRUE) override;
  int TestCapability(const char* cap) override;
  OGRErr SetAttributeFilter(const char* filter) override;
  void SetSpatialFilter(OGRGeometry* geom) override;
  void SetSpatialFilter(int geom, OGRGeometry* filter) override;
  void SetSpatialFilterRect(double min_x, double min_y, double max_x,
                            double max_y) override;
  void SetSpatialFilterRect(int geom, double min_x, double min_y, double max_x,
                            double max_y) override;
  OGRGeometry* GetSpatialFilter() override;
  const char* GetFIDColumn() override;
  const char* GetGeometryColumn() override;
  OGRErr StartTransaction() override;
  OGRErr CommitTransaction() override;
  OGRErr RollbackTransaction() override;
  OGRErr SyncToDisk() override;
  GDALDataset* GetDataset() override;

 private:
  void write_metadata();

  OGRLayer* inner_ = nullptr;
  SdbdDataset* owner_ = nullptr;
  gis::SmtFeatureType feature_type_ = gis::SmtFtUnknown;
  std::string style_hint_;
};

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_SDBD_LAYER_H_
