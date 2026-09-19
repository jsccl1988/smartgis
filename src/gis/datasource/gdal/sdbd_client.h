// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_CLIENT_H_
#define SDB_DATASOURCE_GDAL_SDBD_CLIENT_H_

#include <memory>
#include <string>

#include "gis/datasource/gdal/ogr_export.h"
#include "net/http/http.h"
#include "net/rpc/rpc.h"

namespace gis {
namespace datasource {

// Outcome of one mogu sdbd call (HTTP or FnRPC). Mirrors net::HttpResult shape.
// RPC success maps to status 200; RPC failure maps to status 0 with error filled.
struct MoguHttpResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
};

// Transport selected from the connection string scheme.
enum class SdbdTransport {
  kHttp,
  kRpc,
};

// Parsed connection for HTTP (:8021 /api/v1/sdbd/*) or FnRPC (:9032).
struct SdbdConnection {
  SdbdTransport transport = SdbdTransport::kHttp;
  std::string http_base;  // stripped trailing '/'
  std::string rpc_host = "127.0.0.1";
  int rpc_port = 9032;
};

// Thin mogu sdbd client. Supports HTTP and FnRPC (SDBD-WSL-TRANSPORT).
// Default HTTP base is http://127.0.0.1:8021 (not the local SdbdHandler prefix).
class SDE_GDAL_EXPORT SdbdClient {
 public:
  // Parses connection: http(s)://… → HTTP; sdbd-rpc://host:port → FnRPC.
  // Empty string uses sdbd_default_base_url().
  explicit SdbdClient(std::string connection = {});

  static SdbdClient from_http(std::string base_url);
  static SdbdClient from_rpc(std::string host_port_or_url);

  SdbdTransport transport() const { return conn_.transport; }
  const SdbdConnection& connection() const { return conn_; }

  void set_base_url(std::string base_url);
  // HTTP: scheme+host; RPC: sdbd-rpc://host:port display form.
  const std::string& base_url() const { return display_; }

  // Absolute URL for a mogu-relative path (leading '/'). HTTP only; for tests.
  std::string url_for(const std::string& path) const;

  MoguHttpResult get_capabilities(int timeout_sec = 5);
  MoguHttpResult get_collections(int timeout_sec = 5);
  MoguHttpResult get_collection(const std::string& id, int timeout_sec = 5);
  MoguHttpResult get_collection_items(const std::string& id,
                                      const std::string& query = "",
                                      int timeout_sec = 5);
  MoguHttpResult get_coverage(const std::string& id, int timeout_sec = 5);
  MoguHttpResult post_query(const std::string& json_body, int timeout_sec = 30);
  MoguHttpResult post_analyze(const std::string& json_body,
                              int timeout_sec = 60);
  MoguHttpResult post_jobs(const std::string& json_body, int timeout_sec = 30);
  MoguHttpResult get_job(const std::string& id, int timeout_sec = 5);
  MoguHttpResult post_scan(const std::string& json_body, int timeout_sec = 60);
  MoguHttpResult post_tile_window(const std::string& json_body,
                                  int timeout_sec = 60);
  MoguHttpResult post_import(const std::string& json_body,
                             int timeout_sec = 120);
  MoguHttpResult post_import_url(const std::string& json_body,
                                 int timeout_sec = 120);
  MoguHttpResult post_ingest(const std::string& json_body,
                             int timeout_sec = 120);

 private:
  explicit SdbdClient(SdbdConnection conn);

  MoguHttpResult get_path(const std::string& path, int timeout_sec);
  MoguHttpResult post_path(const std::string& path, const std::string& body,
                           int timeout_sec);
  MoguHttpResult rpc_call(const char* method, const std::string& json,
                          int timeout_sec);
  bool ensure_rpc(int timeout_sec);
  void refresh_display();
  static MoguHttpResult from_http(const net::HttpResult& http);
  static MoguHttpResult from_rpc_result(const net::RpcResult& rpc);

  SdbdConnection conn_;
  std::string display_;
  net::HttpClient http_;
  std::unique_ptr<net::RpcClient> rpc_;
  bool rpc_connected_ = false;
};

// Env SG_SDBD_BASE if set and non-empty; else http://127.0.0.1:8021.
SDE_GDAL_EXPORT std::string sdbd_default_base_url();

// Env SG_SDBD_RPC if set and non-empty; else 127.0.0.1:9032 (host:port).
SDE_GDAL_EXPORT std::string sdbd_default_rpc_endpoint();

// Parse http(s):// or sdbd-rpc:// (default RPC port 9032). Bare host:port → RPC.
SDE_GDAL_EXPORT SdbdConnection parse_sdbd_connection(const std::string& url);

// FnRPC method names aligned with HTTP resources (SDBD-WSL-RPC-KEYS). Public
// for unit tests — no live server required.
SDE_GDAL_EXPORT const char* sdbd_rpc_method_capabilities();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_collections();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_collections_get();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_collections_items();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_coverage_get();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_query();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_analyze();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_jobs();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_jobs_get();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_scan();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_tile_window();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_import();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_import_url();
SDE_GDAL_EXPORT const char* sdbd_rpc_method_ingest();

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_SDBD_CLIENT_H_
