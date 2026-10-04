// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/ogr/ogr_connect.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "gis/datasource/sdbd/sdbd_driver.h"
#include "gis/datasource/sdbd/sdbd_endpoint.h"

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

std::string make_db_file_open_target(const ConnectionSpec& spec) {
  return join_dir_file(spec.service.c_str(), spec.db_name.c_str());
}

std::string make_file_open_target(const ConnectionSpec& spec) {
  return join_dir_file(spec.path.c_str(), spec.file_name.c_str());
}

std::string make_postgres_open_target(const ConnectionSpec& spec) {
  std::string host = spec.service;
  std::string port = "5432";
  const std::string::size_type colon = host.rfind(':');
  if (colon != std::string::npos && colon + 1 < host.size()) {
    port = host.substr(colon + 1);
    host = host.substr(0, colon);
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "PG:host=%s port=%s dbname=%s user=%s password=%s",
           host.c_str(), port.c_str(), spec.db_name.c_str(), spec.uid.c_str(),
           spec.pwd.c_str());
  return buf;
}

}  // namespace

template <uint Provider>
std::string db_provider_traits<Provider>::open_target(
    const ConnectionSpec& /*spec*/) {
  return std::string();
}

template <uint Provider>
std::string file_provider_traits<Provider>::open_target(
    const ConnectionSpec& /*spec*/) {
  return std::string();
}

template <uint Provider>
std::string mem_provider_traits<Provider>::open_target(
    const ConnectionSpec& /*spec*/) {
  return std::string();
}

std::string db_provider_traits<gis::PROVIDER_GPKG>::open_target(
    const ConnectionSpec& spec) {
  return make_db_file_open_target(spec);
}

std::string db_provider_traits<gis::PROVIDER_SPATIALITE>::open_target(
    const ConnectionSpec& spec) {
  return make_db_file_open_target(spec);
}

std::string db_provider_traits<gis::PROVIDER_POSTGRES>::open_target(
    const ConnectionSpec& spec) {
  return make_postgres_open_target(spec);
}

std::string db_provider_traits<gis::PROVIDER_SDBD>::open_target(
    const ConnectionSpec& spec) {
  return sdbd_base_url(spec);
}

std::string sdbd_base_url(const ConnectionSpec& spec) {
  if (!spec.url.empty()) {
    return spec.url;
  }
  return sdbd_default_base_url();
}

std::string file_provider_traits<gis::PROVIDER_SHAPE>::open_target(
    const ConnectionSpec& spec) {
  return make_file_open_target(spec);
}

std::string file_provider_traits<gis::PROVIDER_OGR_SUPPORT>::open_target(
    const ConnectionSpec& spec) {
  return make_file_open_target(spec);
}

template <typename Fn>
bool visit_db_provider(uint provider, Fn&& fn) {
  switch (provider) {
    case gis::PROVIDER_GPKG:
      return fn(db_provider_traits<gis::PROVIDER_GPKG>{});
    case gis::PROVIDER_POSTGRES:
      return fn(db_provider_traits<gis::PROVIDER_POSTGRES>{});
    case gis::PROVIDER_SPATIALITE:
      return fn(db_provider_traits<gis::PROVIDER_SPATIALITE>{});
    case gis::PROVIDER_SDBD:
      return fn(db_provider_traits<gis::PROVIDER_SDBD>{});
    default:
      return false;
  }
}

template <typename Fn>
bool visit_file_provider(uint provider, Fn&& fn) {
  switch (provider) {
    case gis::PROVIDER_SHAPE:
      return fn(file_provider_traits<gis::PROVIDER_SHAPE>{});
    case gis::PROVIDER_OGR_SUPPORT:
      return fn(file_provider_traits<gis::PROVIDER_OGR_SUPPORT>{});
    default:
      return false;
  }
}

bool is_db_provider_supported(uint provider) {
  return visit_db_provider(provider, [](auto traits) {
    return decltype(traits)::supported;
  });
}

bool is_file_provider_supported(uint provider) {
  return visit_file_provider(provider, [](auto traits) {
    return decltype(traits)::supported;
  });
}

bool is_mem_provider_supported(uint provider) {
  return provider == gis::PROVIDER_MEM_VER1;
}

const char* gdal_driver_name(uint provider) {
  const char* name = nullptr;
  visit_db_provider(provider, [&](auto traits) {
    name = decltype(traits)::driver_name;
    return true;
  });
  return name;
}

const char* gdal_driver_name_for(const ConnectionSpec& spec) {
  if (spec.ds_type == gis::DS_MEM) {
    return mem_provider_traits<gis::PROVIDER_MEM_VER1>::driver_name;
  }
  if (is_db_provider_supported(spec.provider_id)) {
    return gdal_driver_name(spec.provider_id);
  }
  if (spec.ds_type == gis::DS_FILE_SMF) {
    const char* name = nullptr;
    if (visit_file_provider(spec.provider_id, [&](auto traits) {
          name = decltype(traits)::driver_name;
          return true;
        })) {
      return name;
    }
    return file_provider_traits<gis::PROVIDER_SHAPE>::driver_name;
  }
  return nullptr;
}

std::string make_gdal_open_target(const ConnectionSpec& spec) {
  if (spec.ds_type == gis::DS_MEM) {
    return mem_provider_traits<gis::PROVIDER_MEM_VER1>::open_target(spec);
  }
  if (is_db_provider_supported(spec.provider_id)) {
    std::string target;
    const bool stock = visit_db_provider(spec.provider_id, [&](auto traits) {
      if (!decltype(traits)::driver_name) {
        return false;
      }
      target = decltype(traits)::open_target(spec);
      return true;
    });
    return stock ? target : std::string();
  }
  if (spec.ds_type == gis::DS_FILE_SMF) {
    std::string target;
    if (visit_file_provider(spec.provider_id, [&](auto traits) {
          target = decltype(traits)::open_target(spec);
          return true;
        })) {
      return target;
    }
    return file_provider_traits<gis::PROVIDER_SHAPE>::open_target(spec);
  }
  return std::string();
}

std::string make_sdbd_open_target(const ConnectionSpec& spec) {
  // PROVIDER_SDBD is opened via open_provider_sdbd_dataset, not SDBD:MEM.
  if (spec.provider_id == gis::PROVIDER_SDBD) {
    return {};
  }
  if (spec.ds_type == gis::DS_WS || spec.ds_type == gis::DS_DB_ODBC ||
      spec.ds_type == gis::DS_DB_MYSQL || spec.ds_type == gis::DS_DB_ORACLE) {
    return {};
  }
  if (spec.ds_type == gis::DS_DB_ADO &&
      !is_db_provider_supported(spec.provider_id)) {
    return {};
  }
  if (spec.ds_type == gis::DS_MEM) {
    const char* name = spec.name.empty() ? "mem" : spec.name.c_str();
    return format_sdbd_open_name("MEM", name);
  }
  const std::string inner = make_gdal_open_target(spec);
  if (inner.empty()) {
    return {};
  }
  const char* drv = gdal_driver_name_for(spec);
  if (!drv || !drv[0]) {
    return format_sdbd_open_name("AUTO", inner);
  }
  return format_sdbd_open_name(drv, inner);
}

}  // namespace datasource
}  // namespace gis
