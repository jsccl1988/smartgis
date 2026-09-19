// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/sdbd_client.h"
#include "gis/datasource/gdal/sdbd_json.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void expect_eq(const std::string& got, const std::string& want,
               const char* msg) {
  if (got != want) {
    std::fprintf(stderr, "FAIL: %s\n  got:  %s\n  want: %s\n", msg, got.c_str(),
                 want.c_str());
    ++g_fails;
  }
}

}  // namespace

int main() {
  using gis::datasource::Json;
  using gis::datasource::SdbdClient;
  using gis::datasource::SdbdTransport;
  using gis::datasource::parse_json;
  using gis::datasource::parse_sdbd_connection;
  using gis::datasource::sdbd_default_base_url;
  using gis::datasource::sdbd_default_rpc_endpoint;
  using gis::datasource::sdbd_rpc_method_analyze;
  using gis::datasource::sdbd_rpc_method_capabilities;
  using gis::datasource::sdbd_rpc_method_collections;
  using gis::datasource::sdbd_rpc_method_collections_get;
  using gis::datasource::sdbd_rpc_method_collections_items;
  using gis::datasource::sdbd_rpc_method_coverage_get;
  using gis::datasource::sdbd_rpc_method_import;
  using gis::datasource::sdbd_rpc_method_import_url;
  using gis::datasource::sdbd_rpc_method_ingest;
  using gis::datasource::sdbd_rpc_method_jobs;
  using gis::datasource::sdbd_rpc_method_jobs_get;
  using gis::datasource::sdbd_rpc_method_query;
  using gis::datasource::sdbd_rpc_method_scan;
  using gis::datasource::sdbd_rpc_method_tile_window;

  // Default base (env may override; pin when unset).
  const char* prev = std::getenv("SG_SDBD_BASE");
  _putenv_s("SG_SDBD_BASE", "");
  expect_eq(sdbd_default_base_url(), "http://127.0.0.1:8021",
            "default base when SG_SDBD_BASE empty");
  _putenv_s("SG_SDBD_BASE", "http://10.0.0.2:9000");
  expect_eq(sdbd_default_base_url(), "http://10.0.0.2:9000",
            "SG_SDBD_BASE override");
  if (prev && prev[0]) {
    _putenv_s("SG_SDBD_BASE", prev);
  } else {
    _putenv_s("SG_SDBD_BASE", "");
  }

  const char* prev_rpc = std::getenv("SG_SDBD_RPC");
  _putenv_s("SG_SDBD_RPC", "");
  expect_eq(sdbd_default_rpc_endpoint(), "127.0.0.1:9032",
            "default RPC endpoint");
  _putenv_s("SG_SDBD_RPC", "10.0.0.3:9032");
  expect_eq(sdbd_default_rpc_endpoint(), "10.0.0.3:9032", "SG_SDBD_RPC override");
  if (prev_rpc && prev_rpc[0]) {
    _putenv_s("SG_SDBD_RPC", prev_rpc);
  } else {
    _putenv_s("SG_SDBD_RPC", "");
  }

  // Connection parse: HTTP vs FnRPC.
  {
    auto http = parse_sdbd_connection("http://127.0.0.1:8021/");
    expect(http.transport == SdbdTransport::kHttp, "http scheme → HTTP");
    expect_eq(http.http_base, "http://127.0.0.1:8021", "http base stripped");

    auto https = parse_sdbd_connection("https://example.com:8443");
    expect(https.transport == SdbdTransport::kHttp, "https scheme → HTTP");

    auto rpc = parse_sdbd_connection("sdbd-rpc://127.0.0.1:9032");
    expect(rpc.transport == SdbdTransport::kRpc, "sdbd-rpc → RPC");
    expect_eq(rpc.rpc_host, "127.0.0.1", "rpc host");
    expect(rpc.rpc_port == 9032, "rpc port 9032");

    auto rpc_def = parse_sdbd_connection("sdbd-rpc://10.1.2.3");
    expect(rpc_def.transport == SdbdTransport::kRpc, "sdbd-rpc host only");
    expect(rpc_def.rpc_port == 9032, "default RPC port 9032");

    auto bare = parse_sdbd_connection("192.168.1.5:9032");
    expect(bare.transport == SdbdTransport::kRpc, "bare host:port → RPC");
    expect_eq(bare.rpc_host, "192.168.1.5", "bare rpc host");
    expect(bare.rpc_port == 9032, "bare rpc port");
  }

  // FnRPC method names (same-key as HTTP resources).
  expect_eq(sdbd_rpc_method_capabilities(), "sdbd.capabilities",
            "rpc capabilities");
  expect_eq(sdbd_rpc_method_collections(), "sdbd.collections",
            "rpc collections");
  expect_eq(sdbd_rpc_method_collections_get(), "sdbd.collections.get",
            "rpc collections.get");
  expect_eq(sdbd_rpc_method_collections_items(), "sdbd.collections.items",
            "rpc collections.items");
  expect_eq(sdbd_rpc_method_coverage_get(), "sdbd.coverage.get",
            "rpc coverage.get");
  expect_eq(sdbd_rpc_method_query(), "sdbd.query", "rpc query");
  expect_eq(sdbd_rpc_method_analyze(), "sdbd.analyze", "rpc analyze");
  expect_eq(sdbd_rpc_method_jobs(), "sdbd.jobs", "rpc jobs");
  expect_eq(sdbd_rpc_method_jobs_get(), "sdbd.jobs.get", "rpc jobs.get");
  expect_eq(sdbd_rpc_method_scan(), "sdbd.scan", "rpc scan");
  expect_eq(sdbd_rpc_method_tile_window(), "sdbd.tile_window",
            "rpc tile_window");
  expect_eq(sdbd_rpc_method_import(), "sdbd.import", "rpc import");
  expect_eq(sdbd_rpc_method_import_url(), "sdbd.import_url", "rpc import_url");
  expect_eq(sdbd_rpc_method_ingest(), "sdbd.ingest", "rpc ingest");

  SdbdClient client("http://127.0.0.1:8021/");
  expect(client.transport() == SdbdTransport::kHttp, "ctor HTTP transport");
  expect_eq(client.base_url(), "http://127.0.0.1:8021",
            "ctor strips trailing slash");

  expect_eq(client.url_for("/api/v1/sdbd/capabilities"),
            "http://127.0.0.1:8021/api/v1/sdbd/capabilities",
            "capabilities url");
  expect_eq(client.url_for("/api/v1/sdbd/collections"),
            "http://127.0.0.1:8021/api/v1/sdbd/collections",
            "collections url");
  expect_eq(client.url_for("/api/v1/sdbd/collections/roads"),
            "http://127.0.0.1:8021/api/v1/sdbd/collections/roads",
            "collection by id url");
  expect_eq(client.url_for("/api/v1/sdbd/collections/roads/items"),
            "http://127.0.0.1:8021/api/v1/sdbd/collections/roads/items",
            "collection items url");
  expect_eq(client.url_for("/api/v1/sdbd/coverage/dem"),
            "http://127.0.0.1:8021/api/v1/sdbd/coverage/dem", "coverage url");
  expect_eq(client.url_for("/api/v1/sdbd/query"),
            "http://127.0.0.1:8021/api/v1/sdbd/query", "query url");
  expect_eq(client.url_for("/api/v1/sdbd/analyze"),
            "http://127.0.0.1:8021/api/v1/sdbd/analyze", "analyze url");
  expect_eq(client.url_for("/api/v1/sdbd/jobs"),
            "http://127.0.0.1:8021/api/v1/sdbd/jobs", "jobs url");
  expect_eq(client.url_for("/api/v1/sdbd/jobs/j1"),
            "http://127.0.0.1:8021/api/v1/sdbd/jobs/j1", "job by id url");
  expect_eq(client.url_for("/api/v1/sdbd/scan"),
            "http://127.0.0.1:8021/api/v1/sdbd/scan", "scan url");
  expect_eq(client.url_for("/api/v1/sdbd/tile_window"),
            "http://127.0.0.1:8021/api/v1/sdbd/tile_window",
            "tile_window url");
  expect_eq(client.url_for("/api/v1/sdbd/import"),
            "http://127.0.0.1:8021/api/v1/sdbd/import", "import url");
  expect_eq(client.url_for("/api/v1/sdbd/import_url"),
            "http://127.0.0.1:8021/api/v1/sdbd/import_url", "import_url url");
  expect_eq(client.url_for("/api/v1/sdbd/ingest"),
            "http://127.0.0.1:8021/api/v1/sdbd/ingest", "ingest url");

  SdbdClient rpc_client = SdbdClient::from_rpc("sdbd-rpc://127.0.0.1:9032");
  expect(rpc_client.transport() == SdbdTransport::kRpc, "from_rpc transport");
  expect_eq(rpc_client.base_url(), "sdbd-rpc://127.0.0.1:9032",
            "from_rpc display");

  // Items path with optional query (via public API against closed port).
  client.set_base_url("http://127.0.0.1:1");
  auto items = client.get_collection_items("roads", "limit=2", /*timeout_sec=*/1);
  expect(!items.ok, "unreachable host → ok==false (items)");
  expect_eq(client.url_for("/api/v1/sdbd/collections/roads/items?limit=2"),
            "http://127.0.0.1:1/api/v1/sdbd/collections/roads/items?limit=2",
            "items with query string");

  auto caps = client.get_capabilities(/*timeout_sec=*/1);
  expect(!caps.ok, "unreachable host → ok==false (capabilities)");

  auto posted = client.post_query("{}", /*timeout_sec=*/1);
  expect(!posted.ok, "unreachable host → ok==false (post_query)");

  // Tiny embedded JSON via existing parse_json (mogu-shaped collections list).
  const char kCollectionsJson[] =
      R"({"collections":[{"id":"roads","title":"Roads"}]})";
  Json root;
  std::string err;
  expect(parse_json(kCollectionsJson, &root, &err), "parse_json collections");
  expect(root.type == Json::kObject, "root object");
  expect(root.has("collections"), "has collections");
  const Json* arr = root.get("collections");
  expect(arr && arr->type == Json::kArray && arr->a.size() == 1,
         "collections array size 1");
  if (arr && !arr->a.empty()) {
    expect_eq(arr->a[0].string("id"), "roads", "collection id");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d FAIL\n", g_fails);
    return 1;
  }
  std::printf("sdbd_client_test ok\n");
  return 0;
}
