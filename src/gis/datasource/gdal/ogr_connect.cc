// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_connect.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gis/datasource/gdal/sdbd_gdal_driver.h"

namespace gis {
namespace datasource {

namespace {

std::string join_dir_file(const char* dir, const char* name) {
  std::string path = dir ? dir : "";
  const std::string file = name ? name : "";
  if (file.empty()) {
    return path;
  }
  if (path.empty()) {
    return file;
  }
  const char last = path.back();
  if (last != '\\' && last != '/') {
    path += '\\';
  }
  path += file;
  return path;
}

std::string make_db_file_open_target(const gis::SmtDataSourceInfo& info) {
  return join_dir_file(info.db.szService, info.db.szDBName);
}

std::string make_file_open_target(const gis::SmtDataSourceInfo& info) {
  return join_dir_file(info.file.szPath, info.file.szFileName);
}

std::string make_postgres_open_target(const gis::SmtDataSourceInfo& info) {
  std::string host = info.db.szService;
  std::string port = "5432";
  const std::string::size_type colon = host.rfind(':');
  if (colon != std::string::npos && colon + 1 < host.size()) {
    port = host.substr(colon + 1);
    host = host.substr(0, colon);
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "PG:host=%s port=%s dbname=%s user=%s password=%s",
           host.c_str(), port.c_str(), info.db.szDBName, info.szUID,
           info.szPWD);
  return buf;
}

}  // namespace

template <uint Provider>
std::string db_provider_traits<Provider>::open_target(
    const gis::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

template <uint Provider>
std::string file_provider_traits<Provider>::open_target(
    const gis::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

template <uint Provider>
std::string mem_provider_traits<Provider>::open_target(
    const gis::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

std::string db_provider_traits<gis::PROVIDER_GPKG>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return make_db_file_open_target(info);
}

std::string db_provider_traits<gis::PROVIDER_SPATIALITE>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return make_db_file_open_target(info);
}

std::string db_provider_traits<gis::PROVIDER_POSTGRES>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return make_postgres_open_target(info);
}

std::string db_provider_traits<gis::PROVIDER_SDBD>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return sdbd_base_url_from_info(info);
}

std::string sdbd_base_url_from_info(const gis::SmtDataSourceInfo& info) {
  if (info.szUrl[0] != '\0') {
    return std::string(info.szUrl);
  }
  if (const char* env = std::getenv("SG_SDBD_BASE")) {
    if (env[0] != '\0') {
      return std::string(env);
    }
  }
  return "http://127.0.0.1:8021";
}

std::string file_provider_traits<gis::PROVIDER_SHAPE>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

std::string file_provider_traits<gis::PROVIDER_OGR_SUPPORT>::open_target(
    const gis::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

bool is_db_provider_supported(uint provider) {
  switch (provider) {
    case gis::PROVIDER_GPKG:
      return db_provider_traits<gis::PROVIDER_GPKG>::supported;
    case gis::PROVIDER_POSTGRES:
      return db_provider_traits<gis::PROVIDER_POSTGRES>::supported;
    case gis::PROVIDER_SPATIALITE:
      return db_provider_traits<gis::PROVIDER_SPATIALITE>::supported;
    case gis::PROVIDER_SDBD:
      return db_provider_traits<gis::PROVIDER_SDBD>::supported;
    default:
      return false;
  }
}

bool is_file_provider_supported(uint provider) {
  return provider == gis::PROVIDER_SHAPE ||
         provider == gis::PROVIDER_OGR_SUPPORT;
}

bool is_mem_provider_supported(uint provider) {
  return provider == gis::PROVIDER_MEM_VER1;
}

const char* gdal_driver_name(uint provider) {
  switch (provider) {
    case gis::PROVIDER_GPKG:
      return db_provider_traits<gis::PROVIDER_GPKG>::driver_name;
    case gis::PROVIDER_POSTGRES:
      return db_provider_traits<gis::PROVIDER_POSTGRES>::driver_name;
    case gis::PROVIDER_SPATIALITE:
      return db_provider_traits<gis::PROVIDER_SPATIALITE>::driver_name;
    default:
      return nullptr;
  }
}

const char* gdal_driver_name_for(const gis::SmtDataSourceInfo& info) {
  if (info.unType == gis::DS_MEM) {
    return mem_provider_traits<gis::PROVIDER_MEM_VER1>::driver_name;
  }
  if (is_db_provider_supported(info.unProvider)) {
    return gdal_driver_name(info.unProvider);
  }
  if (info.unType == gis::DS_FILE_SMF) {
    if (info.unProvider == gis::PROVIDER_OGR_SUPPORT) {
      return file_provider_traits<gis::PROVIDER_OGR_SUPPORT>::driver_name;
    }
    return file_provider_traits<gis::PROVIDER_SHAPE>::driver_name;
  }
  return nullptr;
}

std::string make_gdal_open_target(const gis::SmtDataSourceInfo& info) {
  if (info.unType == gis::DS_MEM) {
    return mem_provider_traits<gis::PROVIDER_MEM_VER1>::open_target(info);
  }
  if (is_db_provider_supported(info.unProvider)) {
    switch (info.unProvider) {
      case gis::PROVIDER_GPKG:
        return db_provider_traits<gis::PROVIDER_GPKG>::open_target(info);
      case gis::PROVIDER_POSTGRES:
        return db_provider_traits<gis::PROVIDER_POSTGRES>::open_target(info);
      case gis::PROVIDER_SPATIALITE:
        return db_provider_traits<gis::PROVIDER_SPATIALITE>::open_target(info);
      case gis::PROVIDER_SDBD:
        // Remote HTTP/RPC — not a local GDAL open path.
        return {};
      default:
        break;
    }
  }
  if (info.unType == gis::DS_FILE_SMF) {
    if (info.unProvider == gis::PROVIDER_OGR_SUPPORT) {
      return file_provider_traits<gis::PROVIDER_OGR_SUPPORT>::open_target(info);
    }
    return file_provider_traits<gis::PROVIDER_SHAPE>::open_target(info);
  }
  return std::string();
}

std::string make_sdbd_open_target(const gis::SmtDataSourceInfo& info) {
  // PROVIDER_SDBD is opened via open_provider_sdbd_dataset, not SDBD:MEM.
  if (info.unProvider == gis::PROVIDER_SDBD) {
    return {};
  }
  if (info.unType == gis::DS_WS || info.unType == gis::DS_DB_ODBC ||
      info.unType == gis::DS_DB_MYSQL || info.unType == gis::DS_DB_ORACLE) {
    return {};
  }
  if (info.unType == gis::DS_DB_ADO &&
      !is_db_provider_supported(info.unProvider)) {
    return {};
  }
  if (info.unType == gis::DS_MEM) {
    const char* name = info.szName[0] ? info.szName : "mem";
    return std::string(kSdbdPrefix) + "MEM:" + name;
  }
  const std::string inner = make_gdal_open_target(info);
  if (inner.empty()) {
    return {};
  }
  const char* drv = gdal_driver_name_for(info);
  if (!drv || !drv[0]) {
    return std::string(kSdbdPrefix) + "AUTO:" + inner;
  }
  return std::string(kSdbdPrefix) + drv + ":" + inner;
}

}  // namespace datasource
}  // namespace gis
