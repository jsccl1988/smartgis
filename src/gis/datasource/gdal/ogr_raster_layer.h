// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
#define SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_

#include <string>

#include "gis/datasource/gdal/ogr_export.h"
#include "gis/layer/layer.h"

class GDALDataset;

namespace gis {
namespace datasource {

// Raster / child-image layer backed by a GDALDataset (MEM or file).
// Encoded product blobs (CxImage codes) live in /vsimem; bands + geotransform
// hang on the dataset. Does not invent ADO geom_points tables.
class SDE_GDAL_EXPORT OgrRasterLayer : public gis::SmtRasterLayer {
 public:
  explicit OgrRasterLayer(GDALDataset* owner = nullptr);
  ~OgrRasterLayer() override;

  bool Create() override;
  bool Open(const char* szLayerArchiveName) override;
  bool Close() override;
  bool Fetch(gis::eSmtFetchType type = gis::FETCH_ALL) override;
  void CalEnvelope() override;

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const base::fRect& fLocRect, long lImageCode) override;
  long SetRasterRect(const base::fRect& fLocRect) override;
  long GetRaster(char*& pRasterBuf, long& lRasterBufSize, base::fRect& fLocRect,
                 long& lImageCode) const override;
  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                        base::fRect& fLocRect, long& lImageCode) const override;
  long GetRasterRect(base::fRect& fLocRect) const override;

 private:
  void release_owned_dataset();
  void unlink_blob();
  bool ensure_mem_dataset(int width, int height);
  void apply_geotransform();
  void sync_rect_from_dataset();
  void backfill_blob_from_path(const char* path);
  std::string blob_path() const;

  bool owns_dataset_ = false;
  long image_code_ = -1;
  base::fRect rect_{};
  std::string vsimem_blob_;
};

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
