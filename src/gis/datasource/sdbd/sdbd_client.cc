// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/sdbd/sdbd_client.h"

#include <cstdlib>
#include <utility>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gis {
namespace datasource {
namespace {

constexpr const char kDefaultRpcHost[] = "127.0.0.1";
constexpr int kDefaultRpcPort = 9032;
constexpr const char kJsonContentType[] = "application/json";

std::string strip_trailing_slashes(std::string s) {
  while (!s.empty() && s.back() == '/') {
    s.pop_back();
  }
  return s;
}

bool starts_with_ci(const std::string& s, const char* prefix) {
  const size_t n = std::char_traits<char>::length(prefix);
  if (s.size() < n) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    char a = s[i];
    char b = prefix[i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

std::string items_path(const std::string& id, const std::string& query) {
  std::string path = "/api/v1/sdbd/collections/" + id + "/items";
  if (query.empty()) {
    return path;
  }
  if (query.front() == '?') {
    path += query;
  } else {
    path += '?';
    path += query;
  }
  return path;
}

void write_string(rapidjson::Writer<rapidjson::StringBuffer>& w,
                  const std::string& s) {
  w.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
}

std::string json_id_object(const std::string& id) {
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("id");
  write_string(w, id);
  w.EndObject();
  return std::string(buf.GetString(), buf.GetSize());
}

std::string json_id_query_object(const std::string& id,
                                 const std::string& query) {
  if (query.empty()) {
    return json_id_object(id);
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("id");
  write_string(w, id);
  w.Key("query");
  write_string(w, query);
  w.EndObject();
  return std::string(buf.GetString(), buf.GetSize());
}

void parse_host_port(const std::string& host_port, std::string* host,
                     int* port, int default_port) {
  const auto colon = host_port.rfind(':');
  if (colon != std::string::npos && colon + 1 < host_port.size()) {
    bool digits = true;
    for (size_t i = colon + 1; i < host_port.size(); ++i) {
      if (host_port[i] < '0' || host_port[i] > '9') {
        digits = false;
        break;
      }
    }
    if (digits) {
      *host = host_port.substr(0, colon);
      *port = std::atoi(host_port.c_str() + colon + 1);
      if (*port <= 0) {
        *port = default_port;
      }
      return;
    }
  }
  *host = host_port;
  *port = default_port;
}

}  // namespace

const char* sdbd_rpc_method_capabilities() { return "sdbd.capabilities"; }
const char* sdbd_rpc_method_collections() { return "sdbd.collections"; }
const char* sdbd_rpc_method_collections_get() {
  return "sdbd.collections.get";
}
const char* sdbd_rpc_method_collections_items() {
  return "sdbd.collections.items";
}
const char* sdbd_rpc_method_coverage_get() { return "sdbd.coverage.get"; }
const char* sdbd_rpc_method_query() { return "sdbd.query"; }
const char* sdbd_rpc_method_analyze() { return "sdbd.analyze"; }
const char* sdbd_rpc_method_jobs() { return "sdbd.jobs"; }
const char* sdbd_rpc_method_jobs_get() { return "sdbd.jobs.get"; }
const char* sdbd_rpc_method_scan() { return "sdbd.scan"; }
const char* sdbd_rpc_method_tile_window() { return "sdbd.tile_window"; }
const char* sdbd_rpc_method_import() { return "sdbd.import"; }
const char* sdbd_rpc_method_import_url() { return "sdbd.import_url"; }
const char* sdbd_rpc_method_ingest() { return "sdbd.ingest"; }

std::string sdbd_default_rpc_endpoint() {
  if (const char* env = std::getenv("SG_SDBD_RPC")) {
    if (env[0] != '\0') {
      return std::string(env);
    }
  }
  return std::string(kDefaultRpcHost) + ":" + std::to_string(kDefaultRpcPort);
}

SdbdConnection parse_sdbd_connection(const std::string& url) {
  SdbdConnection out;
  std::string s = strip_trailing_slashes(url);
  if (s.empty()) {
    out.transport = SdbdTransport::kHttp;
    out.http_base = strip_trailing_slashes(sdbd_default_base_url());
    return out;
  }
  if (starts_with_ci(s, "sdbd-rpc://")) {
    out.transport = SdbdTransport::kRpc;
    const std::string rest = s.substr(11);
    parse_host_port(rest, &out.rpc_host, &out.rpc_port, kDefaultRpcPort);
    if (out.rpc_host.empty()) {
      out.rpc_host = kDefaultRpcHost;
    }
    return out;
  }
  if (starts_with_ci(s, "http://") || starts_with_ci(s, "https://")) {
    out.transport = SdbdTransport::kHttp;
    out.http_base = s;
    return out;
  }
  // Bare host:port → FnRPC (SG_SDBD_RPC style).
  out.transport = SdbdTransport::kRpc;
  parse_host_port(s, &out.rpc_host, &out.rpc_port, kDefaultRpcPort);
  if (out.rpc_host.empty()) {
    out.rpc_host = kDefaultRpcHost;
  }
  return out;
}

SdbdClient::SdbdClient(std::string connection)
    : SdbdClient(parse_sdbd_connection(
          connection.empty() ? sdbd_default_base_url() : std::move(connection))) {
}

SdbdClient::SdbdClient(SdbdConnection conn) : conn_(std::move(conn)) {
  if (conn_.transport == SdbdTransport::kHttp && conn_.http_base.empty()) {
    conn_.http_base = strip_trailing_slashes(sdbd_default_base_url());
  }
  refresh_display();
}

SdbdClient SdbdClient::from_http(std::string base_url) {
  SdbdConnection c;
  c.transport = SdbdTransport::kHttp;
  c.http_base = strip_trailing_slashes(std::move(base_url));
  return SdbdClient(std::move(c));
}

SdbdClient SdbdClient::from_rpc(std::string host_port_or_url) {
  return SdbdClient(parse_sdbd_connection(std::move(host_port_or_url)));
}

void SdbdClient::refresh_display() {
  if (conn_.transport == SdbdTransport::kRpc) {
    display_ = std::string("sdbd-rpc://") + conn_.rpc_host + ":" +
               std::to_string(conn_.rpc_port);
  } else {
    display_ = conn_.http_base;
  }
}

void SdbdClient::set_base_url(std::string base_url) {
  conn_ = parse_sdbd_connection(std::move(base_url));
  rpc_.reset();
  rpc_connected_ = false;
  refresh_display();
}

std::string SdbdClient::url_for(const std::string& path) const {
  const std::string& base =
      conn_.transport == SdbdTransport::kHttp ? conn_.http_base : display_;
  if (path.empty()) {
    return base;
  }
  if (path.front() == '/') {
    return base + path;
  }
  return base + '/' + path;
}

SdbdCallResult SdbdClient::from_http(const net::HttpResult& http) {
  SdbdCallResult out;
  out.ok = http.ok;
  out.status = http.status;
  out.body = http.body;
  out.error = http.error;
  return out;
}

SdbdCallResult SdbdClient::from_rpc_result(const net::RpcResult& rpc) {
  SdbdCallResult out;
  if (rpc.ok) {
    out.ok = true;
    out.status = 200;
    out.body = rpc.json;
    return out;
  }
  out.ok = false;
  out.status = 0;
  out.error = rpc.error.empty() ? "rpc error" : rpc.error;
  if (rpc.error_code != 0) {
    out.error += " (code=";
    out.error += std::to_string(rpc.error_code);
    out.error += ')';
  }
  return out;
}

bool SdbdClient::ensure_rpc(int timeout_sec) {
  if (rpc_connected_ && rpc_) {
    return true;
  }
  if (!rpc_) {
    rpc_ = std::make_unique<net::RpcClient>();
  }
  rpc_->set_timeout_ms(timeout_sec > 0 ? timeout_sec * 1000 : 5000);
  net::RpcEndpoint ep;
  ep.host = conn_.rpc_host;
  ep.port = conn_.rpc_port;
  rpc_connected_ = rpc_->connect(ep);
  return rpc_connected_;
}

SdbdCallResult SdbdClient::rpc_call(const char* method, const std::string& json,
                                    int timeout_sec) {
  if (!ensure_rpc(timeout_sec)) {
    SdbdCallResult out;
    out.ok = false;
    out.status = 0;
    out.error = std::string("rpc connect failed: ") + display_;
    return out;
  }
  rpc_->set_timeout_ms(timeout_sec > 0 ? timeout_sec * 1000 : 5000);
  return from_rpc_result(rpc_->call(method, json));
}

SdbdCallResult SdbdClient::get_path(const std::string& path, int timeout_sec) {
  if (conn_.transport == SdbdTransport::kRpc) {
    SdbdCallResult out;
    out.ok = false;
    out.status = 0;
    out.error = "internal: get_path on RPC transport";
    return out;
  }
  return from_http(http_.get(url_for(path), timeout_sec));
}

SdbdCallResult SdbdClient::post_path(const std::string& path,
                                     const std::string& body,
                                     int timeout_sec) {
  if (conn_.transport == SdbdTransport::kRpc) {
    SdbdCallResult out;
    out.ok = false;
    out.status = 0;
    out.error = "internal: post_path on RPC transport";
    return out;
  }
  return from_http(
      http_.post(url_for(path), body, kJsonContentType, timeout_sec));
}

SdbdCallResult SdbdClient::call_transport(const char* rpc_method,
                                          const std::string& http_path,
                                          TransportVerb verb,
                                          const std::string& payload,
                                          int timeout_sec) {
  if (conn_.transport == SdbdTransport::kRpc) {
    return rpc_call(rpc_method, payload, timeout_sec);
  }
  if (verb == TransportVerb::kPost) {
    return post_path(http_path, payload, timeout_sec);
  }
  return get_path(http_path, timeout_sec);
}

SdbdCallResult SdbdClient::get_capabilities(int timeout_sec) {
  return call_transport(sdbd_rpc_method_capabilities(),
                        "/api/v1/sdbd/capabilities", TransportVerb::kGet, "{}",
                        timeout_sec);
}

SdbdCallResult SdbdClient::get_collections(int timeout_sec) {
  return call_transport(sdbd_rpc_method_collections(),
                        "/api/v1/sdbd/collections", TransportVerb::kGet, "{}",
                        timeout_sec);
}

SdbdCallResult SdbdClient::get_collection(const std::string& id,
                                          int timeout_sec) {
  return call_transport(sdbd_rpc_method_collections_get(),
                        "/api/v1/sdbd/collections/" + id, TransportVerb::kGet,
                        json_id_object(id), timeout_sec);
}

SdbdCallResult SdbdClient::get_collection_items(const std::string& id,
                                                const std::string& query,
                                                int timeout_sec) {
  return call_transport(sdbd_rpc_method_collections_items(),
                        items_path(id, query), TransportVerb::kGet,
                        json_id_query_object(id, query), timeout_sec);
}

SdbdCallResult SdbdClient::get_coverage(const std::string& id,
                                        int timeout_sec) {
  return call_transport(sdbd_rpc_method_coverage_get(),
                        "/api/v1/sdbd/coverage/" + id, TransportVerb::kGet,
                        json_id_object(id), timeout_sec);
}

SdbdCallResult SdbdClient::post_query(const std::string& json_body,
                                      int timeout_sec) {
  return call_transport(sdbd_rpc_method_query(), "/api/v1/sdbd/query",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_analyze(const std::string& json_body,
                                        int timeout_sec) {
  return call_transport(sdbd_rpc_method_analyze(), "/api/v1/sdbd/analyze",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_jobs(const std::string& json_body,
                                     int timeout_sec) {
  return call_transport(sdbd_rpc_method_jobs(), "/api/v1/sdbd/jobs",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::get_job(const std::string& id, int timeout_sec) {
  return call_transport(sdbd_rpc_method_jobs_get(), "/api/v1/sdbd/jobs/" + id,
                        TransportVerb::kGet, json_id_object(id), timeout_sec);
}

SdbdCallResult SdbdClient::post_scan(const std::string& json_body,
                                     int timeout_sec) {
  return call_transport(sdbd_rpc_method_scan(), "/api/v1/sdbd/scan",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_tile_window(const std::string& json_body,
                                            int timeout_sec) {
  return call_transport(sdbd_rpc_method_tile_window(),
                        "/api/v1/sdbd/tile_window", TransportVerb::kPost,
                        json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_import(const std::string& json_body,
                                       int timeout_sec) {
  return call_transport(sdbd_rpc_method_import(), "/api/v1/sdbd/import",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_import_url(const std::string& json_body,
                                           int timeout_sec) {
  return call_transport(sdbd_rpc_method_import_url(), "/api/v1/sdbd/import_url",
                        TransportVerb::kPost, json_body, timeout_sec);
}

SdbdCallResult SdbdClient::post_ingest(const std::string& json_body,
                                       int timeout_sec) {
  return call_transport(sdbd_rpc_method_ingest(), "/api/v1/sdbd/ingest",
                        TransportVerb::kPost, json_body, timeout_sec);
}

}  // namespace datasource
}  // namespace gis
