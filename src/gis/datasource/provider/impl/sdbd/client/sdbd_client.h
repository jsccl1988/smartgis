// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_CLIENT_SDBD_CLIENT_H_
#define GIS_DATASOURCE_SDBD_CLIENT_SDBD_CLIENT_H_

#include <memory>
#include <string>

#include "gis/gis_export.h"
#include "gis/datasource/provider/impl/sdbd/sdbd_endpoint.h"
#include "net/http/http.h"
#include "net/rpc/rpc.h"

namespace gis {
namespace datasource {

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
// Default HTTP base is http://127.0.0.1:8021 (mogu /api/v1/sdbd/*).
class GIS_EXPORT SdbdClient {
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

  SdbdCallResult get_capabilities(int timeout_sec = 5);
  SdbdCallResult get_collections(int timeout_sec = 5);
  SdbdCallResult get_collection(const std::string& id, int timeout_sec = 5);
  SdbdCallResult get_collection_items(const std::string& id,
                                      const std::string& query = "",
                                      int timeout_sec = 5);
  SdbdCallResult get_coverage(const std::string& id, int timeout_sec = 5);
  SdbdCallResult post_query(const std::string& json_body, int timeout_sec = 30);
  SdbdCallResult post_analyze(const std::string& json_body,
                              int timeout_sec = 60);
  SdbdCallResult post_jobs(const std::string& json_body, int timeout_sec = 30);
  SdbdCallResult get_job(const std::string& id, int timeout_sec = 5);
  SdbdCallResult post_scan(const std::string& json_body, int timeout_sec = 60);
  SdbdCallResult post_tile_window(const std::string& json_body,
                                  int timeout_sec = 60);
  SdbdCallResult post_import(const std::string& json_body,
                             int timeout_sec = 120);
  SdbdCallResult post_import_url(const std::string& json_body,
                                 int timeout_sec = 120);
  SdbdCallResult post_ingest(const std::string& json_body,
                             int timeout_sec = 120);

 private:
  explicit SdbdClient(SdbdConnection conn);

  enum class TransportVerb { kGet, kPost };

  // HTTP uses verb + http_path. FnRPC always calls rpc_method with payload
  // ("{}" for GETs that have no JSON body).
  SdbdCallResult call_transport(const char* rpc_method,
                                const std::string& http_path,
                                TransportVerb verb, const std::string& payload,
                                int timeout_sec);

  SdbdCallResult get_path(const std::string& path, int timeout_sec);
  SdbdCallResult post_path(const std::string& path, const std::string& body,
                           int timeout_sec);
  SdbdCallResult rpc_call(const char* method, const std::string& json,
                          int timeout_sec);
  bool ensure_rpc(int timeout_sec);
  void refresh_display();
  static SdbdCallResult from_http(const net::HttpResult& http);
  static SdbdCallResult from_rpc_result(const net::RpcResult& rpc);

  SdbdConnection conn_;
  std::string display_;
  net::HttpClient http_;
  std::unique_ptr<net::RpcClient> rpc_;
  bool rpc_connected_ = false;
};

// Env SG_SDBD_RPC if set and non-empty; else 127.0.0.1:9032 (host:port).
GIS_EXPORT std::string sdbd_default_rpc_endpoint();

// Parse http(s):// or sdbd-rpc:// (default RPC port 9032). Bare host:port → RPC.
GIS_EXPORT SdbdConnection parse_sdbd_connection(const std::string& url);

// FnRPC method names aligned with HTTP resources (SDBD-WSL-RPC-KEYS). Public
// for unit tests — no live server required.
GIS_EXPORT const char* sdbd_rpc_method_capabilities();
GIS_EXPORT const char* sdbd_rpc_method_collections();
GIS_EXPORT const char* sdbd_rpc_method_collections_get();
GIS_EXPORT const char* sdbd_rpc_method_collections_items();
GIS_EXPORT const char* sdbd_rpc_method_coverage_get();
GIS_EXPORT const char* sdbd_rpc_method_query();
GIS_EXPORT const char* sdbd_rpc_method_analyze();
GIS_EXPORT const char* sdbd_rpc_method_jobs();
GIS_EXPORT const char* sdbd_rpc_method_jobs_get();
GIS_EXPORT const char* sdbd_rpc_method_scan();
GIS_EXPORT const char* sdbd_rpc_method_tile_window();
GIS_EXPORT const char* sdbd_rpc_method_import();
GIS_EXPORT const char* sdbd_rpc_method_import_url();
GIS_EXPORT const char* sdbd_rpc_method_ingest();

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_CLIENT_SDBD_CLIENT_H_
