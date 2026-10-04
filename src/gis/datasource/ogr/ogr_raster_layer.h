// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
#define SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_

#include <string>

#include "gis/envelope.h"
#include "gis/gis_export.h"

class GDALDataset;

namespace gis {
namespace datasource {

inline constexpr long k_raster_ok = 0;
inline constexpr long k_raster_fail = 1;
inline constexpr long k_raster_invalid = 2;

// Raster / child-image layer backed by a GDALDataset (MEM or file).
class GIS_EXPORT OgrRasterLayer {
 public:
  explicit OgrRasterLayer(GDALDataset* owner = nullptr);
  ~OgrRasterLayer();

  GDALDataset* dataset() { return owner_ds_; }
  const GDALDataset* dataset() const { return owner_ds_; }

  bool Create();
  bool Open(const char* szLayerArchiveName);
  bool Close();
  bool Fetch();
  bool IsOpen() const { return open_; }
  void CalEnvelope();

  void SetLayerName(const char* szName);
  const char* GetLayerName() const { return name_.c_str(); }
  void get_envelope(Envelope& env) const { env = envelope_; }
  void SetLayerRect(const Envelope& lyr_rect);

  long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                     const Envelope& loc, long lImageCode);
  long SetRasterRect(const Envelope& loc);
  long GetRaster(char*& pRasterBuf, long& lRasterBufSize, Envelope& loc,
                 long& lImageCode) const;
  long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize, Envelope& loc,
                        long& lImageCode) const;
  long GetRasterRect(Envelope& loc) const;

 private:
  void release_owned_dataset();
  void unlink_blob();
  bool ensure_mem_dataset(int width, int height);
  void apply_geotransform();
  void sync_rect_from_dataset();
  void backfill_blob_from_path(const char* path);
  std::string blob_path() const;

  GDALDataset* owner_ds_ = nullptr;
  Envelope envelope_;
  std::string name_;
  bool open_ = false;
  bool owns_dataset_ = false;
  long image_code_ = -1;
  Envelope rect_{};
  std::string vsimem_blob_;
};

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_RASTER_LAYER_H_
