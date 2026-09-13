// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_dataset.h"

#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_feature_kind.h"
#include "sdb/datasource/gdal/ogr_raster_layer.h"
#include "sdb/datasource/gdal/ogr_vec_layer.h"

#include "logmanager.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>
#include <filesystem>
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

// This gdal_sdk build often omits GPKG/SQLite/PostgreSQL. Multi-layer
// file tests then use an ESRI Shapefile directory beside the .gpkg path.
std::string shapefile_dir_from_file_target(const std::string& target) {
  std::string dir = target;
  const std::string::size_type slash = dir.find_last_of("\\/");
  const std::string::size_type dot = dir.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
    dir.resize(dot);
  }
  return dir;
}

GDALDriver* file_create_driver(uint provider, std::string* open_path) {
  const char* want = gdal_driver_name(provider);
  GDALDriver* drv =
      want ? GetGDALDriverManager()->GetDriverByName(want) : nullptr;
  if (drv) {
    return drv;
  }
  if (provider == Smt_GIS::PROVIDER_GPKG ||
      provider == Smt_GIS::PROVIDER_SPATIALITE) {
    gdal_log("GPKG/SQLite driver not in this GDAL; using ESRI Shapefile");
    log_vector_drivers();
    drv = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (drv && open_path) {
      *open_path = shapefile_dir_from_file_target(*open_path);
    }
  } else if (provider == Smt_GIS::PROVIDER_POSTGRES) {
    gdal_log("PostgreSQL driver not in this GDAL");
    log_vector_drivers();
  }
  return drv;
}

Smt_GIS::SmtFeatureType feature_type_from_layer(OGRLayer* lyr) {
  if (!lyr) {
    return Smt_GIS::SmtFtUnknown;
  }
  const OGRwkbGeometryType wkb = wkbFlatten(lyr->GetGeomType());
  if (wkb == wkbPoint && lyr->FindFieldIndex("anno", TRUE) >= 0) {
    return Smt_GIS::SmtFtAnno;
  }
  if (wkb == wkbMultiPoint && lyr->FindFieldIndex("grid_row", TRUE) >= 0) {
    return Smt_GIS::SmtFtGrid;
  }
  switch (wkb) {
    case wkbPoint:
      return Smt_GIS::SmtFtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return Smt_GIS::SmtFtCurve;
    case wkbPolygon:
      return lyr->FindFieldIndex("area", TRUE) >= 0 ? Smt_GIS::SmtFtSurface
                                                   : Smt_GIS::SmtFtTin;
    case wkbMultiPolygon:
    case wkbTIN:
      return lyr->FindFieldIndex("area", TRUE) >= 0 ? Smt_GIS::SmtFtSurface
                                                   : Smt_GIS::SmtFtTin;
    case wkbMultiPoint:
      return Smt_GIS::SmtFtGrid;
    default:
      return Smt_GIS::SmtFtUnknown;
  }
}

}  // namespace

OgrDataSource::OgrDataSource() : dataset_(nullptr) {}

OgrDataSource::~OgrDataSource() {
  Close();
}

bool OgrDataSource::Create() {
  return Open();
}

bool OgrDataSource::Open() {
  if (m_bOpen) {
    Close();
  }
  if (!is_db_provider_supported(m_dsInfo.unProvider)) {
    gdal_log("ACCESS/SQL Server providers are unsupported; use GPKG, "
             "PostgreSQL, or SpatiaLite");
    m_bOpen = false;
    return false;
  }

  GDALAllRegister();
  install_cpl_handler_once();

  const std::string target = make_gdal_open_target(m_dsInfo);
  if (target.empty()) {
    m_bOpen = false;
    return false;
  }

  std::string open_path = target;
  GDALDriver* file_drv = file_create_driver(m_dsInfo.unProvider, &open_path);

  dataset_ = static_cast<GDALDataset*>(GDALOpenEx(
      open_path.c_str(), GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_UPDATE,
      nullptr, nullptr, nullptr));
  if (!dataset_ && file_drv &&
      (m_dsInfo.unProvider == Smt_GIS::PROVIDER_GPKG ||
       m_dsInfo.unProvider == Smt_GIS::PROVIDER_SPATIALITE)) {
    char** opts = nullptr;
    if (m_dsInfo.unProvider == Smt_GIS::PROVIDER_SPATIALITE &&
        std::strcmp(file_drv->GetDescription(), "SQLite") == 0) {
      opts = CSLSetNameValue(opts, "SPATIALITE", "YES");
    }
    if (std::strcmp(file_drv->GetDescription(), "ESRI Shapefile") == 0) {
      std::error_code ec;
      std::filesystem::create_directories(open_path, ec);
    }
    dataset_ = file_drv->Create(open_path.c_str(), 0, 0, 0, GDT_Unknown, opts);
    CSLDestroy(opts);
  }
  if (!dataset_) {
    gdal_log(CPLGetLastErrorMsg());
    m_bOpen = false;
    return false;
  }

  m_bOpen = true;
  fill_layer_infos();
  return true;
}

bool OgrDataSource::Close() {
  if (dataset_) {
    GDALClose(dataset_);
    dataset_ = nullptr;
  }
  m_bOpen = false;
  m_vLayerInfos.clear();
  return true;
}

Smt_GIS::SmtDataSource* OgrDataSource::Clone() const {
  auto* ds = new OgrDataSource();
  ds->SetInfo(m_dsInfo);
  return ds;
}

void OgrDataSource::fill_layer_infos() {
  m_vLayerInfos.clear();
  if (!dataset_) {
    return;
  }
  const int n = dataset_->GetLayerCount();
  for (int i = 0; i < n; ++i) {
    OGRLayer* lyr = dataset_->GetLayer(i);
    if (!lyr) {
      continue;
    }
    Smt_GIS::SmtLayerInfo info;
    std::strncpy(info.szName, lyr->GetName(), MAX_LAYER_NAME - 1);
    std::strncpy(info.szArchiveName, lyr->GetName(), MAX_LAYER_ARCHIVE_NAME - 1);
    info.unFeatureType = feature_type_from_layer(lyr);
    m_vLayerInfos.push_back(info);
  }
}

Smt_GIS::SmtVectorLayer* OgrDataSource::CreateVectorLayer(
    const char* szName, Smt_Core::fRect& lyrRect,
    Smt_GIS::SmtFeatureType ftType) {
  if (!m_bOpen || !dataset_ || !szName) {
    return nullptr;
  }
  auto* layer = new OgrVectorLayer(this);
  layer->SetLayerFeatureType(ftType);
  layer->SetLayerName(szName);
  layer->SetLayerRect(lyrRect);
  if (!layer->Create()) {
    delete layer;
    return nullptr;
  }
  Smt_GIS::SmtLayerInfo info;
  std::strncpy(info.szName, szName, MAX_LAYER_NAME - 1);
  std::strncpy(info.szArchiveName, szName, MAX_LAYER_ARCHIVE_NAME - 1);
  info.unFeatureType = ftType;
  m_vLayerInfos.push_back(info);
  return layer;
}

Smt_GIS::SmtVectorLayer* OgrDataSource::OpenVectorLayer(const char* szName) {
  if (!m_bOpen || !dataset_ || !szName) {
    return nullptr;
  }
  auto* layer = new OgrVectorLayer(this);
  if (!layer->Open(szName)) {
    delete layer;
    return nullptr;
  }
  Smt_GIS::SmtLayerInfo info{};
  GetLayerInfo(info, szName);
  if (info.szName[0] != '\0') {
    layer->SetLayerFeatureType(
        static_cast<Smt_GIS::SmtFeatureType>(info.unFeatureType));
  }
  layer->SetLayerName(szName);
  layer->Fetch();
  return layer;
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
  auto* layer = new OgrRasterLayer(this);
  layer->SetLayerName(szName);
  layer->SetLayerRect(lyrRect);
  if (!layer->Create()) {
    return layer;
  }
  return layer;
}

Smt_GIS::SmtRasterLayer* OgrDataSource::OpenRasterLayer(const char* szName) {
  auto* layer = new OgrRasterLayer(this);
  if (layer->Open(szName)) {
    return layer;
  }
  delete layer;
  return nullptr;
}

bool OgrDataSource::DeleteRasterLayer(const char* /*szName*/) {
  return false;
}

Smt_GIS::SmtTileLayer* OgrDataSource::CreateTileLayer(const char* /*szName*/,
                                                      Smt_Core::fRect& /*lyrRect*/,
                                                      long /*lImageCode*/) {
  return nullptr;
}

Smt_GIS::SmtTileLayer* OgrDataSource::OpenTileLayer(const char* /*szName*/) {
  return nullptr;
}

bool OgrDataSource::DeleteTileLayer(const char* /*szName*/) {
  return false;
}

}  // namespace datasource
}  // namespace sdb
