// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/sdbd_gdal_driver.h"

#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>
#include <string>

namespace sdb {
namespace datasource {
namespace {

bool starts_with_ci(const char* s, const char* prefix) {
  if (!s || !prefix) {
    return false;
  }
  return EQUALN(s, prefix, std::strlen(prefix));
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
  GDALDriver* drv = find_vector_driver(driver_name);
  if (!drv) {
    CPLError(CE_Failure, CPLE_AppDefined,
             "SDBD: driver %s not in this GDAL",
             driver_name ? driver_name : "?");
    return nullptr;
  }
  if (EQUAL(driver_name, "Memory") || EQUAL(driver_name, "MEM")) {
    return create_memory_dataset(target);
  }
  const unsigned flags =
      GDAL_OF_VECTOR | GDAL_OF_RASTER | (update ? GDAL_OF_UPDATE : GDAL_OF_READONLY);
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

// SDBD: | SDBD:MEM[:name] | SDBD:GPKG:path | SDBD:PostgreSQL:PG:... |
// sdbd://host:port/... (in-process Memory; mgis URL, no HTTP sidecar).
GDALDataset* open_inner(const char* filename, bool update) {
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

  if (rest.empty() || EQUAL(rest.c_str(), "MEM") || EQUAL(rest.c_str(), "Memory")) {
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

class SdbdDataset final : public GDALDataset {
 public:
  explicit SdbdDataset(GDALDataset* inner) : inner_(inner) {
    if (inner_) {
      nRasterXSize = inner_->GetRasterXSize();
      nRasterYSize = inner_->GetRasterYSize();
    }
  }

  ~SdbdDataset() override {
    if (inner_) {
      GDALClose(inner_);
      inner_ = nullptr;
    }
  }

  int GetLayerCount() override {
    return inner_ ? inner_->GetLayerCount() : 0;
  }

  OGRLayer* GetLayer(int i) override {
    return inner_ ? inner_->GetLayer(i) : nullptr;
  }

  OGRLayer* GetLayerByName(const char* name) override {
    return inner_ ? inner_->GetLayerByName(name) : nullptr;
  }

  OGRErr DeleteLayer(int i) override {
    if (!inner_) {
      return OGRERR_FAILURE;
    }
    return inner_->DeleteLayer(i);
  }

  int TestCapability(const char* cap) override {
    if (!inner_) {
      return FALSE;
    }
    return inner_->TestCapability(cap);
  }

  static int identify(GDALOpenInfo* info) {
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

  static GDALDataset* open(GDALOpenInfo* info) {
    if (!identify(info)) {
      return nullptr;
    }
    const bool update = (info->nOpenFlags & GDAL_OF_UPDATE) != 0 ||
                        info->eAccess == GA_Update;
    GDALDataset* inner = open_inner(info->pszFilename, update);
    if (!inner) {
      return nullptr;
    }
    auto* ds = new SdbdDataset(inner);
    ds->SetDescription(info->pszFilename);
    return ds;
  }

  static GDALDataset* create(const char* name, int /*x*/, int /*y*/, int /*bands*/,
                             GDALDataType /*type*/, char** /*opts*/) {
    const char* filename = (name && name[0]) ? name : kSdbdPrefix;
    GDALDataset* inner = open_inner(filename, true);
    if (!inner) {
      return nullptr;
    }
    auto* ds = new SdbdDataset(inner);
    ds->SetDescription(filename);
    return ds;
  }

 protected:
  OGRLayer* ICreateLayer(const char* name,
                         const OGRSpatialReference* srs,
                         OGRwkbGeometryType gtype,
                         char** options) override {
    if (!inner_ || !name) {
      return nullptr;
    }
    return inner_->CreateLayer(name, srs, gtype, options);
  }

 private:
  GDALDataset* inner_ = nullptr;
};

}  // namespace

bool register_sdbd_driver() {
  GDALDriverManager* mgr = GetGDALDriverManager();
  if (!mgr) {
    return false;
  }
  if (mgr->GetDriverByName(kSdbdDriverName)) {
    return true;
  }

  auto* drv = new GDALDriver();
  drv->SetDescription(kSdbdDriverName);
  drv->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
  drv->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
  drv->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
  drv->SetMetadataItem(GDAL_DMD_LONGNAME,
                       "SmartGIS sdbd (mgis catalog/recordset over GDAL)");
  drv->SetMetadataItem(GDAL_DMD_CONNECTION_PREFIX, kSdbdPrefix);
  drv->SetMetadataItem(GDAL_DMD_EXTENSIONS, "");
  drv->SetMetadataItem(GDAL_DMD_HELPTOPIC, "sdbd");
  drv->pfnIdentify = SdbdDataset::identify;
  drv->pfnOpen = SdbdDataset::open;
  drv->pfnCreate = SdbdDataset::create;
  mgr->RegisterDriver(drv);
  return mgr->GetDriverByName(kSdbdDriverName) != nullptr;
}

}  // namespace datasource
}  // namespace sdb
