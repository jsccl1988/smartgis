// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/sdbd_dataset.h"

#include "sdb/datasource/gdal/gdal_driver.h"
#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "sdb/datasource/gdal/sdbd_layer.h"

#include "cpl_string.h"

#include <cstring>

namespace sdb {
namespace datasource {
namespace {

bool starts_with_ci(const char* s, const char* prefix) {
  return s && prefix && EQUALN(s, prefix, std::strlen(prefix));
}

GDALDriver* find_vector_driver(const char* name) {
  if (!name || !name[0]) {
    return nullptr;
  }
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName(name);
  if (drv) {
    return drv;
  }
  if (EQUAL(name, "Memory") || EQUAL(name, "MEM")) {
    drv = GetGDALDriverManager()->GetDriverByName("Memory");
    if (!drv) {
      drv = GetGDALDriverManager()->GetDriverByName("MEM");
    }
  }
  return drv;
}

GDALDataset* create_memory_dataset(const char* name) {
  GDALDriver* drv = find_vector_driver("Memory");
  if (!drv) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "SDBD: Memory driver not in this GDAL");
    return nullptr;
  }
  const char* ds_name = (name && name[0]) ? name : "sdbd";
  GDALDataset* ds = drv->Create(ds_name, 0, 0, 0, GDT_Unknown, nullptr);
  if (ds) {
    return ds;
  }
  return drv->Create(ds_name, 1, 1, 1, GDT_Byte, nullptr);
}

GDALDataset* open_or_create_named(const char* driver_name, const char* target,
                                  bool update) {
  if (EQUAL(driver_name, "AUTO")) {
    const unsigned flags = GDAL_OF_VECTOR | GDAL_OF_RASTER |
                           (update ? GDAL_OF_UPDATE : GDAL_OF_READONLY);
    return static_cast<GDALDataset*>(
        GDALOpenEx(target, flags, nullptr, nullptr, nullptr));
  }
  GDALDriver* drv = find_vector_driver(driver_name);
  if (!drv) {
    CPLError(CE_Failure, CPLE_AppDefined, "SDBD: driver %s not in this GDAL",
             driver_name ? driver_name : "?");
    return nullptr;
  }
  if (EQUAL(driver_name, "Memory") || EQUAL(driver_name, "MEM")) {
    return create_memory_dataset(target);
  }
  const unsigned flags = GDAL_OF_VECTOR | GDAL_OF_RASTER |
                         (update ? GDAL_OF_UPDATE : GDAL_OF_READONLY);
  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx(target, flags, nullptr, nullptr, nullptr));
  if (ds) {
    return ds;
  }
  if (update) {
    ds = drv->Create(target, 0, 0, 0, GDT_Unknown, nullptr);
  }
  if (!ds) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "SDBD: open/create failed driver=%s target=%s: %s", driver_name,
             target ? target : "", CPLGetLastErrorMsg());
  }
  return ds;
}

}  // namespace

GDALDataset* open_inner_stock(const char* filename, bool update) {
  if (!filename) {
    return nullptr;
  }
  std::string rest = filename;
  if (starts_with_ci(filename, kSdbdPrefix)) {
    rest = filename + std::strlen(kSdbdPrefix);
  } else if (starts_with_ci(filename, "sdbd://")) {
    return create_memory_dataset("sdbd");
  }

  while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\')) {
    rest.erase(rest.begin());
  }

  if (rest.empty() || EQUAL(rest.c_str(), "MEM") ||
      EQUAL(rest.c_str(), "Memory")) {
    return create_memory_dataset("sdbd");
  }

  const auto colon = rest.find(':');
  std::string driver = rest;
  std::string target;
  if (colon != std::string::npos) {
    driver = rest.substr(0, colon);
    target = rest.substr(colon + 1);
  }

  if (EQUAL(driver.c_str(), "MEM") || EQUAL(driver.c_str(), "Memory")) {
    return create_memory_dataset(target.empty() ? "sdbd" : target.c_str());
  }
  if (target.empty()) {
    return create_memory_dataset(driver.c_str());
  }
  return open_or_create_named(driver.c_str(), target.c_str(), update);
}

SdbdDataset::SdbdDataset(GDALDataset* inner) : inner_(inner) {
  if (inner_) {
    nRasterXSize = inner_->GetRasterXSize();
    nRasterYSize = inner_->GetRasterYSize();
  }
}

SdbdDataset::~SdbdDataset() {
  drop_wrappers();
  if (inner_) {
    GDALClose(inner_);
    inner_ = nullptr;
  }
}

void SdbdDataset::drop_wrappers() { wrappers_.clear(); }

SdbdLayer* SdbdDataset::wrap(OGRLayer* inner) {
  if (!inner) {
    return nullptr;
  }
  for (auto& w : wrappers_) {
    if (w && w->inner() == inner) {
      return w.get();
    }
  }
  wrappers_.push_back(
      std::make_unique<SdbdLayer>(inner, feature_type_of(inner), this));
  return wrappers_.back().get();
}

int SdbdDataset::GetLayerCount() {
  return inner_ ? inner_->GetLayerCount() : 0;
}

OGRLayer* SdbdDataset::GetLayer(int i) { return sdbd_layer(i); }

OGRLayer* SdbdDataset::GetLayerByName(const char* name) {
  return sdbd_layer_by_name(name);
}

SdbdLayer* SdbdDataset::sdbd_layer(int index) {
  return inner_ ? wrap(inner_->GetLayer(index)) : nullptr;
}

SdbdLayer* SdbdDataset::sdbd_layer_by_name(const char* name) {
  return inner_ ? wrap(inner_->GetLayerByName(name)) : nullptr;
}

SdbdLayer* SdbdDataset::create_sdbd_layer(const char* name,
                                          sdb::SmtFeatureType ft) {
  if (!inner_ || !name) {
    return nullptr;
  }
  OGRLayer* lyr = nullptr;
  if (!create_vector_layer(inner_, name, ft, &lyr) || !lyr) {
    return nullptr;
  }
  SdbdLayer* wrap_lyr = wrap(lyr);
  if (wrap_lyr) {
    wrap_lyr->set_feature_type(ft);
  }
  return wrap_lyr;
}

OGRErr SdbdDataset::DeleteLayer(int i) {
  if (!inner_) {
    return OGRERR_FAILURE;
  }
  OGRLayer* doomed = inner_->GetLayer(i);
  for (auto it = wrappers_.begin(); it != wrappers_.end(); ++it) {
    if (*it && (*it)->inner() == doomed) {
      wrappers_.erase(it);
      break;
    }
  }
  return inner_->DeleteLayer(i);
}

int SdbdDataset::TestCapability(const char* cap) {
  return inner_ ? inner_->TestCapability(cap) : FALSE;
}

CPLErr SdbdDataset::FlushCache(bool at_closing) {
  return inner_ ? inner_->FlushCache(at_closing) : CE_None;
}

const OGRSpatialReference* SdbdDataset::GetSpatialRef() const {
  return inner_ ? inner_->GetSpatialRef() : nullptr;
}

CPLErr SdbdDataset::SetSpatialRef(const OGRSpatialReference* srs) {
  return inner_ ? inner_->SetSpatialRef(srs) : CE_Failure;
}

CPLErr SdbdDataset::GetGeoTransform(double* transform) {
  return inner_ ? inner_->GetGeoTransform(transform)
                : GDALDataset::GetGeoTransform(transform);
}

CPLErr SdbdDataset::SetGeoTransform(double* transform) {
  return inner_ ? inner_->SetGeoTransform(transform) : CE_Failure;
}

CPLErr SdbdDataset::AddBand(GDALDataType type, char** options) {
  return inner_ ? inner_->AddBand(type, options) : CE_Failure;
}

GDALDriver* SdbdDataset::GetDriver() {
  return GetGDALDriverManager()->GetDriverByName(kSdbdDriverName);
}

char** SdbdDataset::GetFileList() {
  return inner_ ? inner_->GetFileList() : nullptr;
}

const OGRSpatialReference* SdbdDataset::GetGCPSpatialRef() const {
  return inner_ ? inner_->GetGCPSpatialRef() : nullptr;
}

int SdbdDataset::GetGCPCount() { return inner_ ? inner_->GetGCPCount() : 0; }

const GDAL_GCP* SdbdDataset::GetGCPs() {
  return inner_ ? inner_->GetGCPs() : nullptr;
}

CPLErr SdbdDataset::SetGCPs(int count, const GDAL_GCP* gcps,
                            const OGRSpatialReference* srs) {
  return inner_ ? inner_->SetGCPs(count, gcps, srs) : CE_Failure;
}

CPLErr SdbdDataset::IRasterIO(GDALRWFlag rw, int x_off, int y_off, int x_size,
                              int y_size, void* data, int buf_x, int buf_y,
                              GDALDataType type, int band_count, int* band_map,
                              GSpacing pixel_space, GSpacing line_space,
                              GSpacing band_space,
                              GDALRasterIOExtraArg* extra) {
  if (!inner_) {
    return CE_Failure;
  }
  return inner_->RasterIO(rw, x_off, y_off, x_size, y_size, data, buf_x, buf_y,
                          type, band_count, band_map, pixel_space, line_space,
                          band_space, extra);
}

OGRLayer* SdbdDataset::ICreateLayer(const char* name,
                                    const OGRSpatialReference* srs,
                                    OGRwkbGeometryType gtype, char** options) {
  if (!inner_ || !name) {
    return nullptr;
  }
  return wrap(inner_->CreateLayer(name, srs, gtype, options));
}

int SdbdDataset::identify(GDALOpenInfo* info) {
  if (!info || !info->pszFilename) {
    return FALSE;
  }
  if (starts_with_ci(info->pszFilename, kSdbdPrefix)) {
    return TRUE;
  }
  if (starts_with_ci(info->pszFilename, "sdbd://")) {
    return TRUE;
  }
  return FALSE;
}

GDALDataset* SdbdDataset::open(GDALOpenInfo* info) {
  if (!identify(info)) {
    return nullptr;
  }
  const bool update = (info->nOpenFlags & GDAL_OF_UPDATE) != 0 ||
                      info->eAccess == GA_Update;
  GDALDataset* inner = open_inner_stock(info->pszFilename, update);
  if (!inner) {
    return nullptr;
  }
  auto* ds = new SdbdDataset(inner);
  ds->SetDescription(info->pszFilename);
  return ds;
}

GDALDataset* SdbdDataset::create(const char* name, int /*x*/, int /*y*/,
                                 int /*bands*/, GDALDataType /*type*/,
                                 char** /*opts*/) {
  const char* filename = (name && name[0]) ? name : kSdbdPrefix;
  GDALDataset* inner = open_inner_stock(filename, true);
  if (!inner) {
    return nullptr;
  }
  auto* ds = new SdbdDataset(inner);
  ds->SetDescription(filename);
  return ds;
}

SdbdDataset* as_sdbd_dataset(GDALDataset* ds) {
  return dynamic_cast<SdbdDataset*>(ds);
}

SdbdLayer* as_sdbd_layer(OGRLayer* layer) {
  return dynamic_cast<SdbdLayer*>(layer);
}

GDALDataset* open_sdbd_dataset(const sdb::SmtDataSourceInfo& info) {
  register_gdal_driver();
  const std::string target = make_sdbd_open_target(info);
  if (target.empty()) {
    return nullptr;
  }
  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      target.c_str(), GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_UPDATE, nullptr,
      nullptr, nullptr));
  if (ds) {
    return ds;
  }
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName(kSdbdDriverName);
  if (!drv) {
    return nullptr;
  }
  return drv->Create(target.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
}

}  // namespace datasource
}  // namespace sdb
