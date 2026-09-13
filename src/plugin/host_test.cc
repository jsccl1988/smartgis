// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/orthogrid/commands.h"
#include "plugin/dem/dem_commands.h"
#include "plugin/legacy_am.h"
#include "plugin/legacy_cmd.h"
#include "tool/legacy_msg.h"
#include "plugin/manager_view.h"
#include "plugin/manifest.h"
#include "plugin/model3d/model3d_commands.h"
#include "plugin/print/print_commands.h"
#include "plugin/proj/proj_commands.h"
#include "plugin/official_key.h"
#include "plugin/processing.h"
#include "plugin/registry.h"
#include "plugin/signature.h"
#include "plugin/signature_test_key.h"
#include "plugin/store.h"
#include "tool/command.h"

#include "httplib.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {
int g_fails = 0;
void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}
void step(const char* name) { std::fprintf(stderr, "STEP: %s\n", name); }
}  // namespace

int main() {
  {
    plugin::Manifest m;
    std::string err;
    const char* json =
        "{\"id\":\"smartgis.dem\",\"name\":\"DEM\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"builtin\",\"author\":\"\","
        "\"description\":\"\"}";
    expect(plugin::parse_manifest(json, &m, &err), "happy manifest");
    expect(m.id == "smartgis.dem", "id");
    expect(m.api_version == 2, "api 2");
    expect(m.kind == plugin::PluginKind::kBuiltin, "kind");
  }
  {
    plugin::Manifest m;
    std::string err;
    expect(!plugin::parse_manifest("{\"name\":\"x\"}", &m, &err),
           "missing id fails");
  }
  {
    plugin::Manifest m;
    std::string err;
    const char* json =
        "{\"id\":\"smartgis.sample_hello\",\"name\":\"H\",\"version\":\"1.0.0\","
        "\"api_version\":1,\"kind\":\"python\",\"author\":\"\","
        "\"description\":\"\",\"entry\":\"plugin.py\"}";
    expect(!plugin::parse_manifest(json, &m, &err),
           "api_version 1 rejected for python");
  }
  {
    plugin::Registry reg;
    plugin::Manifest m;
    m.id = "test.fixture";
    m.name = "F";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kBuiltin;
    expect(reg.add_manifest(m, plugin::TrustClass::kBuiltin), "add builtin");
    expect(reg.set_enabled("test.fixture", true, nullptr), "enable builtin");
    expect(reg.find("test.fixture")->state == plugin::PluginState::kEnabled,
           "enabled");
    expect(reg.set_enabled("test.fixture", false, nullptr), "disable");
    expect(reg.find("test.fixture")->state == plugin::PluginState::kDisabled,
           "disabled");
  }
  {
    plugin::Registry reg;
    plugin::Manifest m;
    m.id = "user.unsigned";
    m.name = "U";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kPython;
    m.entry = "plugin.py";
    expect(reg.add_manifest(m, plugin::TrustClass::kDenied), "add unsigned");
    expect(!reg.set_enabled("user.unsigned", true, nullptr),
           "unsigned without trust");
    expect(reg.trust_unsigned("user.unsigned"), "trust");
    expect(reg.set_enabled("user.unsigned", true, nullptr),
           "enable after trust");
  }
  {
    expect(std::string(plugin::legacy_id_from_stem("plugin_dem")) ==
               "smartgis.dem",
           "dem stem");
    expect(std::string(plugin::legacy_id_from_stem("plugin_orthogrid")) ==
               "smartgis.baogrid",
           "orthogrid stem");
    expect(std::string(plugin::legacy_id_from_stem("plugin_orthogrid")) ==
               "smartgis.baogrid",
           "leftover baogrid stem");
    expect(std::string(plugin::legacy_id_from_stem("FooBar")) == "legacy.foobar",
           "unknown stem");
    expect(std::string(plugin::legacy_id_from_display_name("DEM生成")) ==
               "smartgis.dem",
           "dem display");
  }
  {
    content::EventBus bus;
    tool::CommandCatalog catalog;
    content::PluginHost* host =
        content::create_plugin_host(&catalog, &bus, nullptr);
    int n = 0;
    expect(host->contribute_command(
               "test.fixture", "test.ping", "Ping", "tools",
               [&](const tool::CommandArgs&) {
                 ++n;
                 return true;
               }),
           "contribute");
    expect(host->execute("test.ping", {}), "execute");
    expect(n == 1, "ran");
    host->withdraw("test.fixture");
    expect(!host->execute("test.ping", {}), "withdrawn");
    int opened = 0;
    expect(host->contribute_dialog(
               "test.fixture", {"test.dlg", "Dlg"},
               [&](content::PluginHost*) { ++opened; }),
           "dialog");
    expect(host->open_dialog("test.dlg"), "open");
    expect(opened == 1, "opened");
    expect(!host->open_dialog("missing.dlg"), "missing dialog");
    expect(!host->run_processing("missing.proc", "{}"), "missing proc");
    delete host;
  }
  {
    plugin::Registry reg;
    plugin::Manifest m;
    m.id = "test.mgr";
    m.name = "M";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kBuiltin;
    expect(reg.add_manifest(m, plugin::TrustClass::kBuiltin), "mgr add");
    plugin::ManagerView view(&reg, nullptr);
    view.refresh();
    expect(view.plugin_row_count() == 1, "mgr row");
  }
  {
    std::thread::id submitter = std::this_thread::get_id();
    std::thread::id worker{};
    plugin::ProcessingPool pool(plugin::ProcessingMode::kThread);
    bool done_ok = false;
    expect(pool.submit(
               "dem.tin_from_xyz", "{}",
               [&](content::PluginHost*, std::string_view) {
                 worker = std::this_thread::get_id();
                 return true;
               },
               [&](bool ok, std::string) { done_ok = ok; }),
           "submit");
    pool.flush_for_test();
    expect(worker != submitter, "factory off caller thread");
    expect(done_ok, "done true");
    plugin::ProcessingPool stub(plugin::ProcessingMode::kUtilityStub);
    expect(stub.mode() == plugin::ProcessingMode::kUtilityStub, "stub mode");
  }
  {
    const std::string hello = "hello";
    uint8_t sig[64] = {};
    expect(plugin::SignatureVerifier::sign(hello, plugin::kTestPrivateSeed, sig),
           "sign");
    expect(plugin::SignatureVerifier::verify(hello, sig,
                                             plugin::kOfficialPublicKey),
           "verify");
    sig[0] ^= 1;
    expect(!plugin::SignatureVerifier::verify(hello, sig,
                                              plugin::kOfficialPublicKey),
           "flip sig");
  }
  {
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "smartgis_plugin_store";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);

    auto crc32 = [](const std::string& d) -> uint32_t {
      uint32_t c = 0xffffffffu;
      for (unsigned char b : d) {
        c ^= b;
        for (int i = 0; i < 8; ++i) {
          c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
        }
      }
      return ~c;
    };
    auto put16 = [](std::string* o, uint16_t v) {
      o->push_back(static_cast<char>(v & 0xff));
      o->push_back(static_cast<char>((v >> 8) & 0xff));
    };
    auto put32 = [](std::string* o, uint32_t v) {
      o->push_back(static_cast<char>(v & 0xff));
      o->push_back(static_cast<char>((v >> 8) & 0xff));
      o->push_back(static_cast<char>((v >> 16) & 0xff));
      o->push_back(static_cast<char>((v >> 24) & 0xff));
    };
    auto write_store_zip = [&](const fs::path& path,
                               const std::vector<std::pair<std::string, std::string>>&
                                   files) {
      std::string local;
      std::string central;
      uint16_t count = 0;
      for (const auto& f : files) {
        const uint32_t crc = crc32(f.second);
        const uint32_t sz = static_cast<uint32_t>(f.second.size());
        const uint16_t nlen = static_cast<uint16_t>(f.first.size());
        const uint32_t local_off = static_cast<uint32_t>(local.size());
        local += "PK";
        local += '\x03';
        local += '\x04';
        put16(&local, 20);
        put16(&local, 0);
        put16(&local, 0);
        put16(&local, 0);
        put16(&local, 0);
        put32(&local, crc);
        put32(&local, sz);
        put32(&local, sz);
        put16(&local, nlen);
        put16(&local, 0);
        local += f.first;
        local += f.second;
        central += "PK";
        central += '\x01';
        central += '\x02';
        put16(&central, 20);
        put16(&central, 20);
        put16(&central, 0);
        put16(&central, 0);
        put16(&central, 0);
        put16(&central, 0);
        put32(&central, crc);
        put32(&central, sz);
        put32(&central, sz);
        put16(&central, nlen);
        put16(&central, 0);
        put16(&central, 0);
        put16(&central, 0);
        put16(&central, 0);
        put32(&central, 0);
        put32(&central, local_off);
        central += f.first;
        ++count;
      }
      const uint32_t cd_off = static_cast<uint32_t>(local.size());
      const uint32_t cd_sz = static_cast<uint32_t>(central.size());
      std::string eocd = "PK";
      eocd += '\x05';
      eocd += '\x06';
      put16(&eocd, 0);
      put16(&eocd, 0);
      put16(&eocd, count);
      put16(&eocd, count);
      put32(&eocd, cd_sz);
      put32(&eocd, cd_off);
      put16(&eocd, 0);
      std::ofstream out(path, std::ios::binary);
      const std::string blob = local + central + eocd;
      out.write(blob.data(), static_cast<std::streamsize>(blob.size()));
      return blob;
    };

    const std::string json =
        "{\"id\":\"user.zipplug\",\"name\":\"Z\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"python\",\"author\":\"\","
        "\"description\":\"\",\"entry\":\"plugin.py\"}";
    const fs::path zip_path = tmp / "plug.zip";
    const std::string zip_bytes =
        write_store_zip(zip_path, {{"plugin.json", json}, {"plugin.py", "x"}});
    uint8_t sig[64] = {};
    plugin::SignatureVerifier::sign(zip_bytes, plugin::kTestPrivateSeed, sig);
    const fs::path sig_path = tmp / "plug.zip.sig";
    {
      std::ofstream so(sig_path, std::ios::binary);
      so.write(reinterpret_cast<const char*>(sig), 64);
    }
    plugin::Registry reg;
    plugin::Store store(&reg, (tmp / "plugins").string());
    expect(store.install_zip(zip_path.string(), sig_path.string(), true),
           "install zip");
    expect(fs::exists(tmp / "plugins" / "user.zipplug" / "plugin.json"),
           "extracted");
    const fs::path bad = tmp / "not.zip";
    {
      std::ofstream bo(bad, std::ios::binary);
      bo << "not-a-zip";
    }
    expect(!store.install_zip(bad.string(), sig_path.string(), false),
           "bad zip");
    const fs::path evil = tmp / "evil.zip";
    write_store_zip(evil, {{"../evil.json", "{}"}});
    expect(!store.install_zip(evil.string(), "", false), "escape zip");
    expect(!fs::exists(tmp / "evil.json"), "no escape write");
    expect(store.uninstall("user.zipplug"), "uninstall");
    expect(!fs::exists(tmp / "plugins" / "user.zipplug"), "uninstalled");

    auto to_file_url = [](const fs::path& p) {
      std::string s = p.string();
      for (char& c : s) {
        if (c == '\\') {
          c = '/';
        }
      }
      if (s.size() >= 2 && s[1] == ':') {
        return std::string("file:///") + s;
      }
      return std::string("file://") + s;
    };

    const fs::path idx_zip = tmp / "idxplug.zip";
    const std::string idx_json =
        "{\"id\":\"user.idxplug\",\"name\":\"I\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"python\",\"author\":\"\","
        "\"description\":\"\",\"entry\":\"plugin.py\"}";
    const std::string idx_zip_bytes =
        write_store_zip(idx_zip, {{"plugin.json", idx_json}, {"plugin.py", "x"}});
    uint8_t idx_sig[64] = {};
    plugin::SignatureVerifier::sign(idx_zip_bytes, plugin::kTestPrivateSeed,
                                    idx_sig);
    const fs::path idx_sig_path = tmp / "idxplug.zip.sig";
    {
      std::ofstream so(idx_sig_path, std::ios::binary);
      so.write(reinterpret_cast<const char*>(idx_sig), 64);
    }
    const std::string sha = plugin::SignatureVerifier::sha256_hex(idx_zip_bytes);
    const std::string index_json =
        std::string("{\"api_version\":1,\"name\":\"t\",\"plugins\":[{") +
        "\"id\":\"user.idxplug\",\"name\":\"I\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"python\",\"download_url\":\"" +
        to_file_url(idx_zip) + "\",\"sha256\":\"" + sha +
        "\",\"sig_url\":\"" + to_file_url(idx_sig_path) + "\"}]}";
    const fs::path index_path = tmp / "plugins.json";
    {
      std::ofstream io(index_path, std::ios::binary);
      io << index_json;
    }
    plugin::Registry idx_reg;
    plugin::Store idx_store(&idx_reg, (tmp / "idx_plugins").string());
    expect(idx_store.refresh_index(to_file_url(index_path)), "file index");
    expect(idx_store.install_from_index("user.idxplug"), "install file index");
    expect(fs::exists(tmp / "idx_plugins" / "user.idxplug" / "plugin.json"),
           "file index extracted");

    httplib::Server svr;
    svr.Get("/plugins.json", [&](const httplib::Request&, httplib::Response& res) {
      res.set_content(index_json, "application/json");
    });
    const std::string zip_http_body = idx_zip_bytes;
    svr.Get("/p.zip", [&](const httplib::Request&, httplib::Response& res) {
      res.set_content(zip_http_body, "application/zip");
    });
    const std::string sig_http(reinterpret_cast<const char*>(idx_sig), 64);
    svr.Get("/p.zip.sig", [&](const httplib::Request&, httplib::Response& res) {
      res.set_content(sig_http, "application/octet-stream");
    });
    std::string http_index;
    svr.Get("/http.json", [&](const httplib::Request&, httplib::Response& res) {
      res.set_content(http_index, "application/json");
    });
    const int port = svr.bind_to_any_port("127.0.0.1");
    expect(port > 0, "http bind");
    const std::string origin =
        "http://127.0.0.1:" + std::to_string(port);
    http_index =
        std::string("{\"api_version\":1,\"name\":\"t\",\"plugins\":[{") +
        "\"id\":\"user.idxplug\",\"name\":\"I\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"python\",\"download_url\":\"" +
        origin + "/p.zip\",\"sha256\":\"" + sha + "\",\"sig_url\":\"" +
        origin + "/p.zip.sig\"}]}";
    std::thread th([&svr]() { svr.listen_after_bind(); });
    svr.wait_until_ready();
    plugin::Registry http_reg;
    plugin::Store http_store(&http_reg, (tmp / "http_plugins").string());
    expect(http_store.refresh_index(origin + "/http.json"), "http index");
    expect(http_store.install_from_index("user.idxplug"), "install http index");
    expect(fs::exists(tmp / "http_plugins" / "user.idxplug" / "plugin.json"),
           "http index extracted");
    svr.stop();
    th.join();

    fs::remove_all(tmp, ec);
  }
  {
    step("persist");
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "smartgis_plugin_state";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp);
    const fs::path state = tmp / "plugin_state.json";
    plugin::Manifest m;
    m.id = "user.persist";
    m.name = "P";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kPython;
    m.entry = "plugin.py";
    {
      plugin::Registry reg;
      reg.set_state_path(state.string());
      expect(reg.add_manifest(m, plugin::TrustClass::kDenied), "persist add");
      expect(reg.trust_unsigned("user.persist"), "persist trust");
      expect(reg.set_enabled("user.persist", true, nullptr), "persist enable");
    }
    {
      plugin::Registry reg;
      reg.set_state_path(state.string());
      expect(reg.add_manifest(m, plugin::TrustClass::kDenied), "reload add");
      expect(reg.load_state(), "load state");
      expect(reg.find("user.persist")->trust ==
                 plugin::TrustClass::kUnsignedTrusted,
             "trust persisted");
      expect(reg.find("user.persist")->state == plugin::PluginState::kEnabled,
             "enabled persisted");
    }
    fs::remove_all(tmp, ec);
  }
  {
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    expect(plugin::register_dem(host), "register dem");
    expect(plugin::register_proj(host), "register proj");
    expect(plugin::register_print(host), "register print");
    expect(plugin::register_model3d(host), "register model3d");
    expect(plugin::register_orthogrid(host), "register orthogrid");
    // Dialog factories construct Views widgets; do not execute them headless.
    expect(!host->execute("orthogrid.save_boundary", {}), "orthogrid save empty");
    expect(!host->execute("baogrid.save_boundary", {}), "baogrid save empty");
    delete host;
  }
  {
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgDemLoadTin),
                       "dem.load_tin") == 0,
           "dem tin am");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgPrintPreview),
                       "print.preview") == 0,
           "print am");
    expect(std::strcmp(plugin::command_id_from_am_msg(
                           plugin::kAmMsgOrthogridInputBoundary0),
                       "baogrid.input_boundary_0") == 0,
           "baogrid am");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgModel3dSphere),
                       "model3d.add_sphere") == 0,
           "model3d am");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgProjDoPrj),
                       "proj.do_prj") == 0,
           "proj am");
    expect(std::strcmp(plugin::command_id_from_am_msg(tool::kGtMsgViewRefresh),
                       "view.refresh") == 0,
           "plugin gt refresh");
    expect(std::strcmp(plugin::command_id_from_am_msg(tool::kGtMsgAppendLineString),
                       "edit.append.linestring") == 0,
           "plugin gt append line");
    const long keyed =
        tool::kGtMsgViewRefresh | (static_cast<long>(1) << 16);
    expect(std::strcmp(plugin::command_id_from_am_msg(keyed), "view.refresh") ==
               0,
           "plugin keyed refresh");
    expect(plugin::command_id_from_am_msg(-1) == nullptr, "unknown am");
  }
  {
    step("host_async_processing");
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    plugin::ProcessingPool pool(plugin::ProcessingMode::kThread);
    plugin::attach_host_processing(host, &pool);
    std::thread::id submitter = std::this_thread::get_id();
    std::thread::id worker{};
    expect(host->contribute_processing(
               "test.fixture", {"test.async", "Async"},
               [&](content::PluginHost*, std::string_view) {
                 worker = std::this_thread::get_id();
                 return true;
               }),
           "async contrib");
    expect(host->run_processing("test.async", "{}"), "async queue");
    pool.flush_for_test();
    expect(worker != submitter, "host factory off caller");
    delete host;
  }
  {
    step("proj_factory");
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    expect(plugin::register_proj(host), "register proj factory");
    expect(host->run_processing(
               "proj.transform_xy",
               "{\"L\":120.0,\"B\":36.0,\"scale_ruler\":1}"),
           "transform xy");
    const plugin::TransformXyOutput xy = plugin::consume_transform_xy_output();
    expect(xy.valid, "proj factory wrote xy");
    delete host;
  }
  if (g_fails) {
    std::fprintf(stderr, "plugin_host_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("plugin_host_test: ok");
  return 0;
}
