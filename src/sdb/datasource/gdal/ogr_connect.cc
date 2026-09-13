// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"

#include <cstdio>
#include <string>

namespace sdb {
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

std::string make_db_file_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  return join_dir_file(info.db.szService, info.db.szDBName);
}

std::string make_file_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  return join_dir_file(info.file.szPath, info.file.szFileName);
}

std::string make_postgres_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
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
    const Smt_GIS::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

template <uint Provider>
std::string file_provider_traits<Provider>::open_target(
    const Smt_GIS::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

template <uint Provider>
std::string mem_provider_traits<Provider>::open_target(
    const Smt_GIS::SmtDataSourceInfo& /*info*/) {
  return std::string();
}

std::string db_provider_traits<Smt_GIS::PROVIDER_GPKG>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_db_file_open_target(info);
}

std::string db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_db_file_open_target(info);
}

std::string db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_postgres_open_target(info);
}

std::string file_provider_traits<Smt_GIS::PROVIDER_SHAPE>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

std::string file_provider_traits<Smt_GIS::PROVIDER_OGR_SUPPORT>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

bool is_db_provider_supported(uint provider) {
  switch (provider) {
    case Smt_GIS::PROVIDER_GPKG:
      return db_provider_traits<Smt_GIS::PROVIDER_GPKG>::supported;
    case Smt_GIS::PROVIDER_POSTGRES:
      return db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::supported;
    case Smt_GIS::PROVIDER_SPATIALITE:
      return db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::supported;
    default:
      return false;
  }
}

bool is_file_provider_supported(uint provider) {
  return provider == Smt_GIS::PROVIDER_SHAPE ||
         provider == Smt_GIS::PROVIDER_OGR_SUPPORT;
}

bool is_mem_provider_supported(uint provider) {
  return provider == Smt_GIS::PROVIDER_MEM_VER1;
}

const char* gdal_driver_name(uint provider) {
  switch (provider) {
    case Smt_GIS::PROVIDER_GPKG:
      return db_provider_traits<Smt_GIS::PROVIDER_GPKG>::driver_name;
    case Smt_GIS::PROVIDER_POSTGRES:
      return db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::driver_name;
    case Smt_GIS::PROVIDER_SPATIALITE:
      return db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::driver_name;
    default:
      return nullptr;
  }
}

const char* gdal_driver_name_for(const Smt_GIS::SmtDataSourceInfo& info) {
  if (info.unType == Smt_GIS::DS_MEM) {
    return mem_provider_traits<Smt_GIS::PROVIDER_MEM_VER1>::driver_name;
  }
  if (is_db_provider_supported(info.unProvider)) {
    return gdal_driver_name(info.unProvider);
  }
  if (info.unType == Smt_GIS::DS_FILE_SMF) {
    if (info.unProvider == Smt_GIS::PROVIDER_OGR_SUPPORT) {
      return file_provider_traits<Smt_GIS::PROVIDER_OGR_SUPPORT>::driver_name;
    }
    return file_provider_traits<Smt_GIS::PROVIDER_SHAPE>::driver_name;
  }
  return nullptr;
}

std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  if (info.unType == Smt_GIS::DS_MEM) {
    return mem_provider_traits<Smt_GIS::PROVIDER_MEM_VER1>::open_target(info);
  }
  if (is_db_provider_supported(info.unProvider)) {
    switch (info.unProvider) {
      case Smt_GIS::PROVIDER_GPKG:
        return db_provider_traits<Smt_GIS::PROVIDER_GPKG>::open_target(info);
      case Smt_GIS::PROVIDER_POSTGRES:
        return db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::open_target(
            info);
      case Smt_GIS::PROVIDER_SPATIALITE:
        return db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::open_target(
            info);
      default:
        break;
    }
  }
  if (info.unType == Smt_GIS::DS_FILE_SMF) {
    if (info.unProvider == Smt_GIS::PROVIDER_OGR_SUPPORT) {
      return file_provider_traits<Smt_GIS::PROVIDER_OGR_SUPPORT>::open_target(
          info);
    }
    return file_provider_traits<Smt_GIS::PROVIDER_SHAPE>::open_target(info);
  }
  return std::string();
}

}  // namespace datasource
}  // namespace sdb
