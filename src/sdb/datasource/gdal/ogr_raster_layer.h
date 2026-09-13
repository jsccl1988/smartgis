// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
#define SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_

#include "layer.h"
#include "sdb/datasource/gdal/ogr_export.h"

namespace sdb {
namespace datasource {

class OgrDataSource;

// Raster / child-image layer. Create/Open stay false while band I/O is
// UNSUPPORTED; never invents an ADO geom_points blob table.
class SMT_SDE_GDAL_EXPORT OgrRasterLayer : public Smt_GIS::SmtRasterLayer {
 public:
  explicit OgrRasterLayer(OgrDataSource* owner);
  ~OgrRasterLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(Smt_GIS::eSmtFetchType type = Smt_GIS::FETCH_ALL) override;

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const Smt_Core::fRect& fLocRect, long lImageCode) override;
  long SetRasterRect(const Smt_Core::fRect& fLocRect) override;
  long GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                 Smt_Core::fRect& fLocRect, long& lImageCode) const override;
  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                        Smt_Core::fRect& fLocRect,
                        long& lImageCode) const override;
  long GetRasterRect(Smt_Core::fRect& fLocRect) const override;

 private:
  OgrDataSource* owner_;
  Smt_Core::fRect rect_;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
