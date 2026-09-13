// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
#define SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_

#include "sdb/layer/layer.h"
#include "sdb/datasource/gdal/ogr_export.h"

class GDALDataset;

namespace sdb {
namespace datasource {

// Raster / child-image layer. Create/Open stay false while band I/O is
// UNSUPPORTED; never invents an ADO geom_points blob table.
class SDE_GDAL_EXPORT OgrRasterLayer : public sdb::SmtRasterLayer {
 public:
  explicit OgrRasterLayer(GDALDataset* owner);
  ~OgrRasterLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(sdb::eSmtFetchType type = sdb::FETCH_ALL) override;

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const base::fRect& fLocRect, long lImageCode) override;
  long SetRasterRect(const base::fRect& fLocRect) override;
  long GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                 base::fRect& fLocRect, long& lImageCode) const override;
  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                        base::fRect& fLocRect,
                        long& lImageCode) const override;
  long GetRasterRect(base::fRect& fLocRect) const override;

 private:
  base::fRect rect_;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
