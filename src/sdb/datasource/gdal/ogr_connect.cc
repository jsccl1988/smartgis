// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"

#include <cstdio>
#include <string>

namespace sdb {
namespace datasource {

namespace {

std::string make_file_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  std::string path = info.db.szService;
  if (!path.empty()) {
    char last = path[path.size() - 1];
    if (last != '\\' && last != '/') {
      path += '\\';
    }
  }
  path += info.db.szDBName;
  return path;
}

std::string make_postgres_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  std::string host = info.db.szService;
  std::string port = "5432";
  std::string::size_type colon = host.rfind(':');
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

std::string db_provider_traits<Smt_GIS::PROVIDER_GPKG>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

std::string db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_file_open_target(info);
}

std::string db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::open_target(
    const Smt_GIS::SmtDataSourceInfo& info) {
  return make_postgres_open_target(info);
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

std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  switch (info.unProvider) {
    case Smt_GIS::PROVIDER_GPKG:
      return db_provider_traits<Smt_GIS::PROVIDER_GPKG>::open_target(info);
    case Smt_GIS::PROVIDER_POSTGRES:
      return db_provider_traits<Smt_GIS::PROVIDER_POSTGRES>::open_target(info);
    case Smt_GIS::PROVIDER_SPATIALITE:
      return db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE>::open_target(info);
    default:
      return std::string();
  }
}

}  // namespace datasource
}  // namespace sdb
