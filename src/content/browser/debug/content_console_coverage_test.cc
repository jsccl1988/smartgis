// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/camera/view_frame.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/document/gis_scene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "base/core/log.h"
#include "base/log/log_sink.h"
#include "base/process/switches.h"
#include "tool/draft/draft.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Restores process-wide log severity so later cases are not filtered.
struct LevelGuard {
  base::LogLevel saved = base::log_sink().min_level();
  ~LevelGuard() { base::log_sink().set_min_level(saved); }
};

// Map document plus camera, exposed to the console through DebugAgentHost.
struct ConsoleMap {
  content::GisScene scene;
  content::ViewFrame frame;
  int refreshes = 0;

  void seed() {
    expect(scene.create_layer("roads", "LineString"), "create roads");
    expect(scene.create_layer("places", "Point"), "create places");
    expect(scene.select_layer("roads"), "select roads");
    tool::Draft line{};
    line.kind = tool::DraftKind::kLineString;
    line.points = {{10, 20}, {180, 90}};
    expect(scene.append_from_draft(line, "draw.linestring").len != 0,
           "append road");
    expect(scene.select_layer("places"), "select places");
    tool::Draft point{};
    point.kind = tool::DraftKind::kPoint;
    point.points = {{40, 50}};
    expect(scene.append_from_draft(point, "draw.point").len != 0,
           "append place");
    expect(scene.create_layer("blocks", "Polygon"), "create blocks");
    tool::Draft ring{};
    ring.kind = tool::DraftKind::kPolygon;
    ring.points = {{10, 20}, {180, 20}, {180, 160}, {10, 160}};
    expect(scene.append_from_draft(ring, "draw.polygon").len != 0,
           "append block");
  }

  void bind(content::DebugAgent* agent) {
    content::DebugAgentHost host;
    host.refresh_gis = [this] {
      frame.fit_extent(scene, 800, 600);
      ++refreshes;
    };
    host.extent_string = [this] {
      const content::Extent2 e = scene.world_extent();
      char buf[160];
      std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f,%.4f", e.xmin, e.ymin,
                    e.xmax, e.ymax);
      return std::string(buf);
    };
    host.layer_names = [this] {
      std::vector<std::string> names;
      const auto descs = scene.layer_descs();
      names.reserve(descs.size());
      for (const auto& desc : descs) {
        names.push_back(desc.name);
      }
      return names;
    };
    // ui_* stubs mirror debug_agent_test so coverage owns the surface without
    // a real Views tree.
    host.ui_dump_tree = [] { return std::string("root\n child"); };
    host.ui_overlay_stats = [] { return std::string("unavailable"); };
    host.ui_find = [](const std::string& name) {
      return std::string("found:") + name;
    };
    host.ui_click = [](int x, int y, int button) {
      return std::string("click:") + std::to_string(x) + "," +
             std::to_string(y) + "," + std::to_string(button);
    };
    host.ui_type = [](const std::string& utf8) {
      return std::string("type:") + utf8;
    };
    agent->set_host(std::move(host));
  }
};

std::string recv_line(SOCKET sock) {
  std::string buf;
  char c = 0;
  while (true) {
    const int n = ::recv(sock, &c, 1, 0);
    if (n <= 0) {
      break;
    }
    if (c == '\n') {
      break;
    }
    if (c != '\r') {
      buf.push_back(c);
    }
  }
  return buf;
}

bool send_line(SOCKET sock, const std::string& line) {
  const std::string payload = line + "\n";
  const char* p = payload.c_str();
  int left = static_cast<int>(payload.size());
  while (left > 0) {
    const int n = ::send(sock, p, left, 0);
    if (n <= 0) {
      return false;
    }
    p += n;
    left -= n;
  }
  return true;
}

std::string discovery_path() {
  char buf[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, buf);
  if (n == 0 || n >= MAX_PATH) {
    return "smartgis-debug.json";
  }
  return std::string(buf) + "smartgis-debug.json";
}

// Local :ui surface via DebugAgentHost stubs (no real Views tree).
void expect_ui_local(content::DebugAgent* agent) {
  expect(agent->exec_line(":ui tree").find("root") != std::string::npos,
         ":ui tree");
  expect(agent->exec_line(":ui find btn").find("found:btn") != std::string::npos,
         ":ui find");
  expect(agent->exec_line(":ui click 3 4").find("click:3,4,1") !=
             std::string::npos,
         ":ui click");
  expect(agent->exec_line(":ui type hi").find("type:hi") != std::string::npos,
         ":ui type");
  expect(agent->exec_line(":ui overlay") == "unavailable", ":ui overlay");
}

// Soft note only — never hard-fail; daemon / Python may be absent offline.
void soft_check_optional_backends() {
  // Intentionally do not call :sdbd / :py here: they hit the network or a
  // Python worker and can stall offline CI. Help listing is asserted above.
  std::printf("soft: :sdbd/:py left unbound (optional backends)\n");
  std::fflush(stdout);
}

void expect_local_commands(content::DebugAgent* agent, ConsoleMap* map) {
  expect(agent->exec_line("").empty(), "empty line");
  expect(agent->exec_line("\n").empty(), "newline only");

  const std::string help = agent->exec_line(":help");
  expect(help.find(":help") != std::string::npos, "help lists :help");
  expect(help.find(":clear") != std::string::npos, "help lists :clear");
  expect(help.find(":log.level") != std::string::npos, "help lists :log.level");
  expect(help.find(":refresh") != std::string::npos, "help lists :refresh");
  expect(help.find(":extent") != std::string::npos, "help lists :extent");
  expect(help.find(":layers") != std::string::npos, "help lists :layers");
  expect(help.find(":ui") != std::string::npos, "help lists :ui");
  expect(help.find(":gis") != std::string::npos, "help lists :gis");
  expect(help.find(":rhi") != std::string::npos, "help lists :rhi");
  expect(help.find("test|bench") != std::string::npos, "help lists harness");
  expect(help.find(":sdbd") != std::string::npos, "help lists :sdbd");
  expect(help.find(":py") != std::string::npos, "help lists :py");

  LOGGING(LOG_INFO, "console_coverage_marker");
  expect(agent->exec_line(":clear") == "cleared", ":clear");
  expect(base::log_sink().size() == 0, "sink empty after :clear");

  expect(agent->exec_line(":log.level debug") == "min_level=DEBUG",
         ":log.level debug");
  expect(base::log_sink().min_level() == base::LogLevel::kDebug, "sink debug");
  expect(agent->exec_line(":log.level nope") == "bad level", "bad log level");
  expect(agent->exec_line(":log.level info") == "min_level=INFO",
         ":log.level info");

  const std::string layers = agent->exec_line(":layers");
  expect(layers.find("roads") != std::string::npos, ":layers roads");
  expect(layers.find("places") != std::string::npos, ":layers places");
  expect(layers.find("blocks") != std::string::npos, ":layers blocks");
  expect(layers.find('\n') != std::string::npos, ":layers newline");

  const std::string extent = agent->exec_line(":extent");
  expect(extent.find(',') != std::string::npos, ":extent has commas");
  expect(extent.find("no host") == std::string::npos, ":extent from scene");

  const int before = map->refreshes;
  const double scale_before = map->frame.scale();
  expect(agent->exec_line(":refresh") == "refreshed", ":refresh");
  expect(map->refreshes == before + 1, "refresh hooked");
  expect(map->frame.scale() != scale_before, "fit_extent changed scale");

  expect_ui_local(agent);

  expect(agent->exec_line(":nope").find("unknown command") != std::string::npos,
         "unknown command");

  agent->set_host({});
  expect(agent->exec_line(":refresh") == "no host.refresh_gis",
         "refresh without host");
  expect(agent->exec_line(":extent") == "no host.extent_string",
         "extent without host");
  expect(agent->exec_line(":layers") == "no host.layer_names",
         "layers without host");
  expect(agent->exec_line(":ui tree") == "ui host not bound",
         "ui tree without host");
  map->bind(agent);
}

void expect_socket(content::DebugAgent* agent) {
  expect(agent->start(), "start");
  const int port = agent->port();
  expect(port > 0, "port");
  expect(agent->start(), "start idempotent");
  expect(agent->port() == port, "port stable");
  expect(agent->is_running(), "running");

  const std::string disc = discovery_path();
  {
    std::ifstream in(disc);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string body = ss.str();
    expect(body.find("127.0.0.1") != std::string::npos, "discovery host");
    expect(body.find(std::to_string(port)) != std::string::npos,
           "discovery port");
    expect(body.find(std::to_string(GetCurrentProcessId())) !=
               std::string::npos,
           "discovery pid");
  }

  SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  expect(sock != INVALID_SOCKET, "socket");
  DWORD timeout_ms = 2000;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<u_short>(port));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  expect(::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0,
         "connect");

  expect(send_line(sock, "{\"id\":1,\"method\":\"ping\",\"params\":{}}"),
         "send ping");
  const std::string ping = recv_line(sock);
  expect(ping.find("\"ok\":true") != std::string::npos, "ping ok");
  expect(ping.find("pong") != std::string::npos, "ping pong");

  expect(send_line(sock, "{\"id\":2}"), "send missing method");
  const std::string missing = recv_line(sock);
  expect(missing.find("\"ok\":false") != std::string::npos, "missing method");
  expect(missing.find("missing method") != std::string::npos,
         "missing method text");

  expect(send_line(sock, "not-json"), "send bad json");
  const std::string bad_json = recv_line(sock);
  expect(bad_json.find("\"ok\":false") != std::string::npos, "bad json");

  expect(send_line(sock,
                   "{\"id\":3,\"method\":\"no.such\",\"params\":{}}"),
         "send unknown");
  const std::string unknown = recv_line(sock);
  expect(unknown.find("unknown method") != std::string::npos, "unknown method");

  LOGGING(LOG_INFO, "console_coverage_tail");
  expect(send_line(sock,
                   "{\"id\":4,\"method\":\"log.tail\",\"params\":{\"n\":20}}"),
         "send tail");
  const std::string tail = recv_line(sock);
  expect(tail.find("\"ok\":true") != std::string::npos, "tail ok");
  expect(tail.find("console_coverage_tail") != std::string::npos, "tail hit");

  expect(send_line(sock,
                   "{\"id\":5,\"method\":\"log.set_level\",\"params\":{"
                   "\"level\":\"error\"}}"),
         "send set_level");
  const std::string set_level = recv_line(sock);
  expect(set_level.find("\"ok\":true") != std::string::npos, "set_level ok");
  expect(base::log_sink().min_level() == base::LogLevel::kError, "level error");
  LOGGING(LOG_INFO, "console_coverage_dropped");
  LOGGING(LOG_ERROR, "console_coverage_kept");
  expect(send_line(sock,
                   "{\"id\":6,\"method\":\"log.tail\",\"params\":{\"n\":30}}"),
         "send tail after filter");
  const std::string filtered = recv_line(sock);
  expect(filtered.find("console_coverage_dropped") == std::string::npos,
         "info dropped");
  expect(filtered.find("console_coverage_kept") != std::string::npos,
         "error kept");

  expect(send_line(sock,
                   "{\"id\":7,\"method\":\"log.set_level\",\"params\":{"
                   "\"level\":\"nope\"}}"),
         "send bad level");
  const std::string bad_level = recv_line(sock);
  expect(bad_level.find("bad level") != std::string::npos, "bad level rpc");

  base::log_sink().set_min_level(base::LogLevel::kTrace);
  expect(send_line(sock,
                   "{\"id\":8,\"method\":\"cmd.exec\",\"params\":{\"line\":"
                   "\":layers\"}}"),
         "send layers");
  const std::string layers = recv_line(sock);
  expect(layers.find("roads") != std::string::npos, "rpc layers roads");
  expect(layers.find("places") != std::string::npos, "rpc layers places");

  expect(send_line(sock,
                   "{\"id\":9,\"method\":\"cmd.exec\",\"params\":{\"line\":"
                   "\":extent\"}}"),
         "send extent");
  const std::string extent = recv_line(sock);
  expect(extent.find("\"ok\":true") != std::string::npos, "rpc extent ok");
  expect(extent.find(",") != std::string::npos, "rpc extent body");

  expect(send_line(sock,
                   "{\"id\":10,\"method\":\"cmd.exec\",\"params\":{\"line\":"
                   "\":refresh\"}}"),
         "send refresh");
  const std::string refresh = recv_line(sock);
  expect(refresh.find("refreshed") != std::string::npos, "rpc refresh");

  expect(send_line(sock,
                   "{\"id\":11,\"method\":\"cmd.exec\",\"params\":{}}"),
         "send empty exec");
  const std::string empty_exec = recv_line(sock);
  expect(empty_exec.find("\"ok\":true") != std::string::npos, "empty exec ok");

  expect(send_line(sock,
                   "{\"id\":12,\"method\":\"ui.dump_tree\",\"params\":{}}"),
         "send ui.dump_tree");
  const std::string ui_tree = recv_line(sock);
  expect(ui_tree.find("\"ok\":true") != std::string::npos, "rpc ui.dump_tree ok");
  expect(ui_tree.find("root") != std::string::npos, "rpc ui.dump_tree body");

  expect(send_line(sock,
                   "{\"id\":13,\"method\":\"ui.find\",\"params\":{"
                   "\"name\":\"btn\"}}"),
         "send ui.find");
  const std::string ui_find = recv_line(sock);
  expect(ui_find.find("found:btn") != std::string::npos, "rpc ui.find");

  expect(send_line(sock,
                   "{\"id\":14,\"method\":\"ui.click\",\"params\":{"
                   "\"x\":3,\"y\":4,\"button\":1}}"),
         "send ui.click");
  const std::string ui_click = recv_line(sock);
  expect(ui_click.find("click:3,4,1") != std::string::npos, "rpc ui.click");

  expect(send_line(sock,
                   "{\"id\":15,\"method\":\"ui.type\",\"params\":{"
                   "\"text\":\"hi\"}}"),
         "send ui.type");
  const std::string ui_type = recv_line(sock);
  expect(ui_type.find("type:hi") != std::string::npos, "rpc ui.type");

  expect(send_line(sock,
                   "{\"id\":16,\"method\":\"ui.overlay_stats\",\"params\":{}}"),
         "send ui.overlay_stats");
  const std::string ui_overlay = recv_line(sock);
  expect(ui_overlay.find("unavailable") != std::string::npos,
         "rpc ui.overlay_stats");

  expect(send_line(sock, "{\"id\":17,\"method\":\"log.subscribe\",\"params\":{}}"),
         "send subscribe");
  const std::string sub = recv_line(sock);
  expect(sub.find("\"ok\":true") != std::string::npos, "subscribe ok");
  LOGGING(LOG_INFO, "console_coverage_push");
  const std::string push = recv_line(sock);
  expect(push.find("\"event\":\"log\"") != std::string::npos, "log event");
  expect(push.find("console_coverage_push") != std::string::npos, "push hit");

  closesocket(sock);
  agent->stop();
  expect(!agent->is_running(), "stopped");
  expect(agent->port() == 0, "port cleared");
  expect(GetFileAttributesA(disc.c_str()) == INVALID_FILE_ATTRIBUTES,
         "discovery removed");
}

void expect_env_flag() {
  const char* prev = base::switch_cstr("debug");
  const std::string saved = prev ? prev : "";
  base::set_switch("debug", "1");
  expect(content::debug_console_env_enabled(), "SG_DEBUG=1");
  base::set_switch("debug", "0");
  expect(!content::debug_console_env_enabled(), "SG_DEBUG=0");
  base::set_switch("debug", saved.c_str());
}

}  // namespace

int main() {
  LevelGuard levels;
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);

  ConsoleMap map;
  map.seed();
  content::DebugAgent agent;
  map.bind(&agent);
  expect_local_commands(&agent, &map);
  soft_check_optional_backends();
  // sdbd.* and py.* hit the network or a Python process; they stay out of
  // the hard-fail path so a missing daemon cannot stall coverage.
  expect_socket(&agent);
  expect_env_flag();

  WSACleanup();
  if (g_fails) {
    std::fprintf(stderr, "content_console_coverage_test: %d failure(s)\n",
                 g_fails);
    return 1;
  }
  std::printf("content_console_coverage_test: ok\n");
  return 0;
}
