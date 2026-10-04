// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>
#include <string>

#include "gis/datasource/gdal/gdal_driver.h"
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

  expect(spec.name == "demo_ds", "spec name");
  expect(spec.url == "sdbd-rpc://127.0.0.1:9032", "spec url");
  expect(spec.ds_type == gis::DS_FILE_SMF, "spec ds_type");
  expect(spec.provider_id == gis::PROVIDER_GPKG, "spec provider_id");

  ConnectionSpec remote;
  remote.provider_id = gis::PROVIDER_SDBD;
  remote.kind = ProviderKind::kRemoteSdbd;
  expect(remote.kind == ProviderKind::kRemoteSdbd, "remote kind");
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
