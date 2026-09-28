// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>
#include <string>

#include "gis/datasource/provider/impl/gdal/gdal_driver.h"
#include "gis/datasource/session/connection_spec.h"
#include "gis/datasource/session/data_session.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_connection_spec_round_trip() {
  using gis::datasource::ConnectionSpec;
  using gis::datasource::ProviderKind;

  ConnectionSpec spec;
  spec.kind = ProviderKind::kLocalSdbd;
  spec.name = "demo_ds";
  spec.url = "sdbd-rpc://127.0.0.1:9032";
  spec.path = "C:\\data";
  spec.file_name = "roads.gpkg";
  spec.uid = "u";
  spec.pwd = "p";
  spec.ds_type = gis::DS_FILE_SMF;
  spec.provider_id = gis::PROVIDER_GPKG;

  gis::SmtDataSourceInfo info = spec.to_info();
  expect(std::strcmp(info.szName, "demo_ds") == 0, "to_info name");
  expect(std::strcmp(info.szUrl, "sdbd-rpc://127.0.0.1:9032") == 0,
         "to_info url");
  expect(info.unType == gis::DS_FILE_SMF, "to_info ds_type");
  expect(info.unProvider == gis::PROVIDER_GPKG, "to_info provider_id");

  ConnectionSpec back = ConnectionSpec::from_info(info);
  expect(back.name == "demo_ds", "from_info name");
  expect(back.url == "sdbd-rpc://127.0.0.1:9032", "from_info url");
  expect(back.kind == ProviderKind::kLocalSdbd, "from_info kind local");
  expect(back.provider_id == gis::PROVIDER_GPKG, "from_info provider_id");

  gis::SmtDataSourceInfo remote;
  remote.unProvider = gis::PROVIDER_SDBD;
  expect(ConnectionSpec::kind_from_info(remote) == ProviderKind::kRemoteSdbd,
         "kind_from_info remote");
}

void test_mem_vector_layer() {
  using gis::datasource::DataSession;

  gis::datasource::register_gdal_driver();
  DataSession session;
  gis::MapLayer layer = session.create_mem_vector_layer("scratch");
  expect(layer.ogr() != nullptr, "mem layer ogr non-null");
  if (layer.ogr()) {
    expect(std::strcmp(layer.name(), "scratch") == 0, "mem layer name");
  }
}

}  // namespace

int main() {
  test_connection_spec_round_trip();
  test_mem_vector_layer();

  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("datasource_session_test ok\n");
  return 0;
}
