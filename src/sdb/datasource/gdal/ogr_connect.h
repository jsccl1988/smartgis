// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
#define SDB_DATASOURCE_GDAL_OGR_CONNECT_H_

#include "layer.h"

#include <string>

namespace sdb {
namespace datasource {

template <uint Provider>
struct db_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<Smt_GIS::PROVIDER_GPKG> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "GPKG";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<Smt_GIS::PROVIDER_POSTGRES> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "PostgreSQL";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<Smt_GIS::PROVIDER_SPATIALITE> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "SQLite";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

bool is_db_provider_supported(uint provider);
const char* gdal_driver_name(uint provider);
std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
