// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Hard live e2e for WSL mogu sdbd (HTTP :8021 + FnRPC :9032). No SKIP.

#include "gis/datasource/gdal/ogr_connect.h"
#include "gis/datasource/gdal/sdbd_client.h"
#include "gis/datasource/gdal/sdbd_json.h"
#include "gis/datasource/mgr/datasource_mgr.h"
#include "gis/layer/layer.h"

#include "gdal_priv.h"
#include "net/rpc/rpc.h"
#include "net/rpc/wire.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <stdio.h>  // _popen / _pclose
#endif

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string mogu_root() {
  if (const char* env = std::getenv("SG_MOGU_ROOT")) {
    if (env[0] != '\0') {
      return env;
    }
  }
  return "/home/ccl/dev/src/mogu";
}

// First IPv4 from `wsl hostname -I` (empty on failure).
std::string wsl_primary_ipv4() {
  FILE* pipe = _popen("wsl -e bash -lc \"hostname -I\"", "r");
  if (!pipe) {
    return {};
  }
  char buf[256] = {};
  if (!std::fgets(buf, sizeof(buf), pipe)) {
    _pclose(pipe);
    return {};
  }
  _pclose(pipe);
  std::string line(buf);
  while (!line.empty() &&
         (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
    line.pop_back();
  }
  const auto sp = line.find(' ');
  if (sp != std::string::npos) {
    line.resize(sp);
  }
  return line;
}

bool http_capabilities_ok(const std::string& base) {
  gis::datasource::SdbdClient client(base);
  const auto caps = client.get_capabilities(/*timeout_sec=*/3);
  return caps.ok && caps.status == 200;
}

// Prefer SG_SDBD_BASE / 127.0.0.1; if WSL localhost relay hangs, fall back to
// the distro eth IP (common on WSL2 without mirrored networking).
std::string resolve_http_base_for_live() {
  const std::string def = gis::datasource::sdbd_default_base_url();
  if (http_capabilities_ok(def)) {
    return def;
  }
  const std::string ip = wsl_primary_ipv4();
  if (!ip.empty()) {
    const std::string via_ip = std::string("http://") + ip + ":8021";
    if (http_capabilities_ok(via_ip)) {
      std::fprintf(stderr,
                   "sdbd_live_test: note — localhost :8021 unreachable; "
                   "using WSL IP %s\n",
                   via_ip.c_str());
      _putenv_s("SG_SDBD_BASE", via_ip.c_str());
      return via_ip;
    }
  }
  return {};
}

int try_start_sdbd_via_wsl() {
  const std::string root = mogu_root();
  // setsid so flow_host survives when the outer `wsl bash -lc` exits (otherwise
  // SIGHUP from the short-lived shell kills the background daemon).
  char cmd[1536];
  std::snprintf(
      cmd, sizeof(cmd),
      "wsl bash -lc \"cd '%s' && setsid bash infra/sdbd/scripts/run.sh start "
      "</dev/null >/tmp/sdbd_smartgis_start.log 2>&1\"",
      root.c_str());
  std::fprintf(stderr, "sdbd_live_test: starting daemon via WSL:\n  %s\n", cmd);
  const int rc = std::system(cmd);
  if (rc != 0) {
    std::fprintf(stderr, "sdbd_live_test: run.sh start exit %d\n", rc);
  }
  return rc;
}

// Returns 0 on success; non-zero hard failure (never SKIP).
int ensure_sdbd_alive() {
  std::string base = resolve_http_base_for_live();
  if (!base.empty()) {
    std::printf("sdbd_live_test: HTTP capabilities already up at %s\n",
                base.c_str());
    return 0;
  }

  try_start_sdbd_via_wsl();
  std::system("wsl bash -lc \"sleep 2\"");

  base = resolve_http_base_for_live();
  if (!base.empty()) {
    std::printf("sdbd_live_test: HTTP capabilities OK after start at %s\n",
                base.c_str());
    return 0;
  }

  std::fprintf(stderr,
               "sdbd_live_test: HARD FAIL — HTTP capabilities unreachable "
               "(tried %s and WSL IP :8021; run.sh under %s)\n",
               gis::datasource::sdbd_default_base_url().c_str(),
               mogu_root().c_str());
  return 1;
}

std::string rpc_endpoint_host_port() {
  return gis::datasource::sdbd_default_rpc_endpoint();
}

gis::datasource::SdbdConnection rpc_conn() {
  const std::string ep = rpc_endpoint_host_port();
  if (ep.find("://") != std::string::npos) {
    return gis::datasource::parse_sdbd_connection(ep);
  }
  return gis::datasource::parse_sdbd_connection(std::string("sdbd-rpc://") + ep);
}

// Probe FnRPC. Unbound methods (error_code==2) are a hard fail — mogu must
// expose same-key sdbd.* methods.
int ensure_rpc_alive() {
  const auto conn = rpc_conn();
  net::RpcClient rpc;
  rpc.set_timeout_ms(5000);
  net::RpcEndpoint ep;
  ep.host = conn.rpc_host;
  ep.port = conn.rpc_port;
  if (!rpc.connect(ep)) {
    std::fprintf(stderr,
                 "sdbd_live_test: HARD FAIL — FnRPC connect failed %s:%d "
                 "(SG_SDBD_RPC=%s)\n",
                 ep.host.c_str(), ep.port, rpc_endpoint_host_port().c_str());
    return 1;
  }

  const net::RpcResult caps =
      rpc.call(gis::datasource::sdbd_rpc_method_capabilities(), "{}");
  if (caps.ok) {
    std::printf("sdbd_live_test: FnRPC sdbd.capabilities OK\n");
    return 0;
  }

  if (caps.error_code == net::detail::k_rpc_not_bound) {
    std::fprintf(
        stderr,
        "sdbd_live_test: HARD FAIL — FnRPC connected at %s:%d but method "
        "'%s' is unbound (error_code=2). mogu must register same-key "
        "sdbd.* methods; TCP-only is not enough.\n",
        ep.host.c_str(), ep.port,
        gis::datasource::sdbd_rpc_method_capabilities());
    return 1;
  }

  std::fprintf(stderr,
               "sdbd_live_test: HARD FAIL — FnRPC call sdbd.capabilities "
               "failed: %s (code=%u)\n",
               caps.error.empty() ? "(no error)" : caps.error.c_str(),
               static_cast<unsigned>(caps.error_code));
  return 1;
}

std::vector<std::string> parse_collection_ids(const std::string& body) {
  std::vector<std::string> ids;
  gis::datasource::Json root;
  std::string err;
  if (!gis::datasource::parse_json(body, &root, &err)) {
    return ids;
  }
  const gis::datasource::Json* arr = root.get("collections");
  if (!arr || arr->type != gis::datasource::Json::kArray) {
    return ids;
  }
  for (const auto& item : arr->a) {
    if (item.type == gis::datasource::Json::kObject) {
      const std::string id = item.string("id");
      if (!id.empty()) {
        ids.push_back(id);
      }
    }
  }
  return ids;
}

}  // namespace

int main() {
  using gis::DataSourceMgr;
  using gis::PROVIDER_SDBD;
  using gis::SmtDataSourceInfo;
  using gis::datasource::SdbdClient;
  using gis::datasource::SdbdTransport;
  using gis::datasource::is_db_provider_supported;
  using gis::datasource::sdbd_base_url_from_info;

  expect(is_db_provider_supported(PROVIDER_SDBD), "PROVIDER_SDBD supported");
  {
    SmtDataSourceInfo info{};
    info.unProvider = PROVIDER_SDBD;
    expect(sdbd_base_url_from_info(info) ==
               gis::datasource::sdbd_default_base_url(),
           "default base from empty szUrl");
    std::strncpy(info.szUrl, "sdbd-rpc://127.0.0.1:9032",
                 sizeof(info.szUrl) - 1);
    expect(sdbd_base_url_from_info(info) == "sdbd-rpc://127.0.0.1:9032",
           "szUrl preserved");
  }

  if (ensure_sdbd_alive() != 0) {
    return 1;
  }

  // HTTP read surface (before FnRPC so unbound methods still leave HTTP evidence).
  SdbdClient http(gis::datasource::sdbd_default_base_url());
  expect(http.transport() == SdbdTransport::kHttp, "default client is HTTP");
  const auto caps = http.get_capabilities();
  expect(caps.ok && caps.status == 200, "HTTP capabilities 200");

  const auto cols = http.get_collections();
  expect(cols.ok && cols.status == 200, "HTTP collections 200");
  const auto ids = parse_collection_ids(cols.body);
  std::printf("sdbd_live_test: %zu collection(s)\n", ids.size());

  bool probed_named = false;
  for (const std::string& id : ids) {
    if (id == "roads" || id == "demo" || id.find("road") != std::string::npos) {
      const auto items = http.get_collection_items(id, "limit=5");
      expect(items.ok && items.status == 200,
             "HTTP items for roads/demo-like collection");
      probed_named = true;
      const auto q = http.post_query(
          std::string("{\"collection\":\"") + id + "\",\"limit\":5}");
      if (!(q.ok && q.status >= 200 && q.status < 300)) {
        std::fprintf(stderr,
                     "note: post_query for %s status=%d err=%s (non-fatal if "
                     "items ok)\n",
                     id.c_str(), q.status, q.error.c_str());
      }
      break;
    }
  }
  if (!probed_named && !ids.empty()) {
    const auto items0 = http.get_collection_items(ids.front(), "limit=2");
    if (!(items0.ok && items0.status == 200)) {
      std::fprintf(stderr,
                   "note: items for first collection '%s' status=%d err=%s "
                   "(no roads/demo id found)\n",
                   ids.front().c_str(), items0.status, items0.error.c_str());
    } else {
      std::printf("sdbd_live_test: items OK for first collection %s\n",
                  ids.front().c_str());
    }
  }

  // DataSourceMgr + PROVIDER_SDBD (HTTP base / SG_SDBD_BASE).
  SmtDataSourceInfo info{};
  info.unType = gis::DS_DB_ADO;
  info.unProvider = PROVIDER_SDBD;
  std::strncpy(info.szName, "sdbd_live_http", sizeof(info.szName) - 1);
  GDALDataset* ds = DataSourceMgr::get_singleton_ptr()->open_dataset(info);
  expect(ds != nullptr, "DataSourceMgr PROVIDER_SDBD open_dataset non-null");
  if (ds) {
    std::printf("sdbd_live_test: HTTP mgr open layers=%d\n", ds->GetLayerCount());
    GDALClose(ds);
  }

  // FnRPC hard check — unbound methods (error_code==2) fail the test (no SKIP).
  if (ensure_rpc_alive() != 0) {
    DataSourceMgr::destroy_instance();
    if (g_fails) {
      std::fprintf(stderr, "sdbd_live_test: %d FAIL (plus FnRPC)\n", g_fails);
    }
    return 1;
  }

  // Also open via sdbd-rpc:// when RPC methods work.
  SmtDataSourceInfo rpc_info{};
  rpc_info.unType = gis::DS_DB_ADO;
  rpc_info.unProvider = PROVIDER_SDBD;
  std::strncpy(rpc_info.szName, "sdbd_live_rpc", sizeof(rpc_info.szName) - 1);
  const std::string rpc_url =
      std::string("sdbd-rpc://") + rpc_conn().rpc_host + ":" +
      std::to_string(rpc_conn().rpc_port);
  std::strncpy(rpc_info.szUrl, rpc_url.c_str(), sizeof(rpc_info.szUrl) - 1);
  GDALDataset* rpc_ds =
      DataSourceMgr::get_singleton_ptr()->open_dataset(rpc_info);
  expect(rpc_ds != nullptr,
         "DataSourceMgr PROVIDER_SDBD via sdbd-rpc:// open non-null");
  if (rpc_ds) {
    std::printf("sdbd_live_test: RPC mgr open layers=%d\n",
                rpc_ds->GetLayerCount());
    GDALClose(rpc_ds);
  }

  DataSourceMgr::destroy_instance();

  if (g_fails) {
    std::fprintf(stderr, "sdbd_live_test: %d FAIL\n", g_fails);
    return 1;
  }
  std::printf("sdbd_live_test ok\n");
  return 0;
}
