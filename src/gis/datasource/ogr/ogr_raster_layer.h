// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_OGR_RASTER_LAYER_H_
#define GIS_DATASOURCE_OGR_RASTER_LAYER_H_

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

  bool create();
  bool open(const char* archive_name);
  bool close();
  bool fetch();
  bool is_open() const { return open_; }
  void cal_envelope();

  void set_name(const char* name);
  const char* name() const { return name_.c_str(); }
  void get_envelope(Envelope& env) const { env = envelope_; }
  void set_rect(const Envelope& lyr_rect);

  long create_raster(const char* buf, long buf_size, const Envelope& loc,
                     long image_code);
  long set_raster_rect(const Envelope& loc);
  long get_raster(char*& buf, long& buf_size, Envelope& loc,
                  long& image_code) const;
  long get_raster_no_clone(char*& buf, long& buf_size, Envelope& loc,
                           long& image_code) const;
  long get_raster_rect(Envelope& loc) const;

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

#endif  // GIS_DATASOURCE_OGR_RASTER_LAYER_H_
