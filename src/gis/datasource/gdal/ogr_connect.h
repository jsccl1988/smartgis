// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
#define SDB_DATASOURCE_GDAL_OGR_CONNECT_H_

#include <string>

#include "gis/layer/layer.h"

namespace gis {
namespace datasource {

template <uint Provider>
struct db_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<gis::PROVIDER_GPKG> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "GPKG";
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<gis::PROVIDER_POSTGRES> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "PostgreSQL";
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct db_provider_traits<gis::PROVIDER_SPATIALITE> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "SQLite";
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

// HTTP/FnRPC remote sdbd — not a stock GDAL driver. open_target returns the
// connection string (szUrl or default HTTP base); mgr opens via SdbdRemoteDataset.
template <>
struct db_provider_traits<gis::PROVIDER_SDBD> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <uint Provider>
struct file_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct file_provider_traits<gis::PROVIDER_SHAPE> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "ESRI Shapefile";
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct file_provider_traits<gis::PROVIDER_OGR_SUPPORT> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <uint Provider>
struct mem_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const gis::SmtDataSourceInfo& info);
};

template <>
struct mem_provider_traits<gis::PROVIDER_MEM_VER1> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "Memory";
  static std::string open_target(const gis::SmtDataSourceInfo&) {
    return "MEM:";
  }
};

bool is_db_provider_supported(uint provider);
bool is_file_provider_supported(uint provider);
bool is_mem_provider_supported(uint provider);
const char* gdal_driver_name(uint provider);
const char* gdal_driver_name_for(const gis::SmtDataSourceInfo& info);
std::string make_gdal_open_target(const gis::SmtDataSourceInfo& info);
std::string make_sdbd_open_target(const gis::SmtDataSourceInfo& info);

// Connection string for PROVIDER_SDBD: info.szUrl if set, else default HTTP base.
std::string sdbd_base_url_from_info(const gis::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
