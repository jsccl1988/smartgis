// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_dataset.h"

#include "sdb/datasource/gdal/gdal_driver.h"
#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "sdb/datasource/gdal/ogr_raster_layer.h"

#include "logmanager.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>
#include <string>

namespace sdb {
namespace datasource {

namespace {

void gdal_log(const char* msg) {
  Smt_Core::SmtLogManager* mgr = Smt_Core::SmtLogManager::GetSingletonPtr();
  if (!mgr) {
    return;
  }
  Smt_Core::SmtLog* log = mgr->GetLog("SmtSDEGdalDevice");
  if (!log) {
    log = mgr->CreateLog("SmtSDEGdalDevice");
  }
  if (log) {
    log->LogMessage("%s", msg);
  }
}

void CPL_STDCALL gdal_cpl_handler(CPLErr /*err*/, int /*num*/,
                                  const char* msg) {
  if (msg) {
    gdal_log(msg);
  }
}

void install_cpl_handler_once() {
  static bool installed = false;
  if (!installed) {
    CPLSetErrorHandler(gdal_cpl_handler);
    installed = true;
  }
}

void log_vector_drivers() {
  GDALDriverManager* mgr = GetGDALDriverManager();
  if (!mgr) {
    return;
  }
  std::string list = "OGR drivers:";
  const int n = mgr->GetDriverCount();
  for (int i = 0; i < n; ++i) {
    GDALDriver* d = mgr->GetDriver(i);
    if (!d || !d->GetMetadataItem(GDAL_DCAP_VECTOR)) {
      continue;
    }
    list += ' ';
    list += d->GetDescription();
  }
  gdal_log(list.c_str());
}

std::string redact_open_target(const std::string& target) {
  std::string s = target;
  const std::string key = "password=";
  const std::string::size_type pos = s.find(key);
  if (pos == std::string::npos) {
    return s;
  }
  std::string::size_type end = s.find(' ', pos);
  if (end == std::string::npos) {
    end = s.size();
  }
  s.replace(pos + key.size(), end - pos - key.size(), "***");
  return s;
}

}  // namespace

OgrDataSource::OgrDataSource() = default;

OgrDataSource::~OgrDataSource() { Close(); }

GDALDataset* OgrDataSource::release() {
  GDALDataset* ds = dataset_;
  dataset_ = nullptr;
  open_ = false;
  return ds;
}

bool OgrDataSource::Create() { return Open(); }

bool OgrDataSource::Open() {
  if (open_) {
    Close();
  }

  register_gdal_driver();
  install_cpl_handler_once();

  if (info_.unType == Smt_GIS::DS_WS || info_.unType == Smt_GIS::DS_DB_ODBC ||
      info_.unType == Smt_GIS::DS_DB_MYSQL ||
      info_.unType == Smt_GIS::DS_DB_ORACLE) {
    gdal_log("datasource type is not an OGR v1 path (WS/ODBC/MySQL/Oracle)");
    open_ = false;
    return false;
  }

  if (info_.unType == Smt_GIS::DS_DB_ADO &&
      !is_db_provider_supported(info_.unProvider)) {
    gdal_log("ACCESS/SQL Server providers are unsupported; use GPKG, "
             "PostgreSQL, or SpatiaLite");
    open_ = false;
    return false;
  }

  if (info_.unType == Smt_GIS::DS_MEM) {
    const char* want =
        mem_provider_traits<Smt_GIS::PROVIDER_MEM_VER1>::driver_name;
    GDALDriver* drv =
        want ? GetGDALDriverManager()->GetDriverByName(want) : nullptr;
    if (!drv) {
      drv = GetGDALDriverManager()->GetDriverByName("MEM");
    }
    if (!drv) {
      gdal_log("Memory driver not in this GDAL");
      log_vector_drivers();
      open_ = false;
      return false;
    }
    const char* name = info_.szName[0] ? info_.szName : "mem";
    dataset_ = drv->Create(name, 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset_) {
      dataset_ = drv->Create(name, 1, 1, 1, GDT_Byte, nullptr);
    }
    if (!dataset_) {
      gdal_log("Memory Create failed");
      gdal_log(CPLGetLastErrorMsg());
      open_ = false;
      return false;
    }
    open_ = true;
    return true;
  }

  const std::string target = make_gdal_open_target(info_);
  if (target.empty()) {
    open_ = false;
    return false;
  }

  const char* want = gdal_driver_name_for(info_);
  GDALDriver* file_drv =
      want ? GetGDALDriverManager()->GetDriverByName(want) : nullptr;
  const bool needs_named_driver =
      info_.unType == Smt_GIS::DS_FILE_SMF
          ? (info_.unProvider != Smt_GIS::PROVIDER_OGR_SUPPORT)
          : is_db_provider_supported(info_.unProvider);
  if (needs_named_driver && !file_drv) {
    std::string fail = "Open/Create failed; driver missing; driver=";
    fail += want ? want : "?";
    fail += "; target=";
    fail += redact_open_target(target);
    gdal_log(fail.c_str());
    log_vector_drivers();
    gdal_log(CPLGetLastErrorMsg());
    open_ = false;
    return false;
  }

  dataset_ = static_cast<GDALDataset*>(GDALOpenEx(
      target.c_str(), GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_UPDATE, nullptr,
      nullptr, nullptr));
  const bool can_create_file =
      file_drv &&
      (info_.unType == Smt_GIS::DS_FILE_SMF ||
       info_.unProvider == Smt_GIS::PROVIDER_GPKG ||
       info_.unProvider == Smt_GIS::PROVIDER_SPATIALITE);
  if (!dataset_ && can_create_file) {
    char** opts = nullptr;
    if (info_.unProvider == Smt_GIS::PROVIDER_SPATIALITE &&
        std::strcmp(file_drv->GetDescription(), "SQLite") == 0) {
      opts = CSLSetNameValue(opts, "SPATIALITE", "YES");
    }
    dataset_ = file_drv->Create(target.c_str(), 0, 0, 0, GDT_Unknown, opts);
    CSLDestroy(opts);
  }
  if (!dataset_) {
    std::string fail = "Open/Create failed; target=";
    fail += redact_open_target(target);
    gdal_log(fail.c_str());
    gdal_log(CPLGetLastErrorMsg());
    open_ = false;
    return false;
  }

  open_ = true;
  return true;
}

bool OgrDataSource::Close() {
  if (dataset_) {
    GDALClose(dataset_);
    dataset_ = nullptr;
  }
  open_ = false;
  return true;
}

OGRLayer* OgrDataSource::CreateVectorLayer(const char* szName,
                                           Smt_Core::fRect& /*lyrRect*/,
                                           Smt_GIS::SmtFeatureType ftType) {
  if (!open_ || !dataset_ || !szName) {
    return nullptr;
  }
  OGRLayer* lyr = nullptr;
  if (!create_vector_layer(dataset_, szName, ftType, &lyr)) {
    return nullptr;
  }
  return lyr;
}

OGRLayer* OgrDataSource::OpenVectorLayer(const char* szName) {
  if (!open_ || !dataset_ || !szName) {
    return nullptr;
  }
  return dataset_->GetLayerByName(szName);
}

bool OgrDataSource::DeleteVectorLayer(const char* szName) {
  if (!dataset_ || !szName) {
    return false;
  }
  for (int i = 0; i < dataset_->GetLayerCount(); ++i) {
    OGRLayer* lyr = dataset_->GetLayer(i);
    if (lyr && std::strcmp(lyr->GetName(), szName) == 0) {
      return dataset_->DeleteLayer(i) == OGRERR_NONE;
    }
  }
  return false;
}

Smt_GIS::SmtRasterLayer* OgrDataSource::CreateRasterLayer(
    const char* szName, Smt_Core::fRect& lyrRect, long /*lImageCode*/) {
  auto* layer = new OgrRasterLayer(dataset_);
  layer->SetLayerName(szName);
  layer->SetLayerRect(lyrRect);
  layer->Create();
  return layer;
}

Smt_GIS::SmtRasterLayer* OgrDataSource::OpenRasterLayer(const char* szName) {
  auto* layer = new OgrRasterLayer(dataset_);
  if (layer->Open(szName)) {
    return layer;
  }
  delete layer;
  return nullptr;
}

}  // namespace datasource
}  // namespace sdb
