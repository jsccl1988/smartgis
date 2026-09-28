// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark for Debug Console local commands + loopback RPC.

#include "content/browser/camera/view_frame.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/document/map_scene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <benchmark/benchmark.h>

#include "tool/draft/draft.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

// Map document plus camera, exposed to the console through DebugAgentHost.
struct ConsoleMap {
  content::MapScene scene;
  content::ViewFrame frame;
  int refreshes = 0;

  void seed() {
    scene.create_layer("roads", "LineString");
    tool::Draft line{};
    line.kind = tool::DraftKind::kLineString;
    line.points = {{10, 20}, {400, 240}};
    scene.append_from_draft(line, "draw.linestring");
    scene.create_layer("blocks", "Polygon");
    tool::Draft ring{};
    ring.kind = tool::DraftKind::kPolygon;
    ring.points = {{10, 20}, {400, 20}, {400, 240}, {10, 240}};
    scene.append_from_draft(ring, "draw.polygon");
  }

  void bind(content::DebugAgent* agent) {
    content::DebugAgentHost host;
    host.refresh_map = [this] {
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
      for (const auto& desc : scene.layer_descs()) {
        names.push_back(desc.name);
      }
      return names;
    };
    agent->set_host(std::move(host));
  }
};

ConsoleMap* g_map = nullptr;
content::DebugAgent* g_agent = nullptr;
SOCKET g_sock = INVALID_SOCKET;

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

void roundtrip(const std::string& line) {
  const char* p = line.c_str();
  int left = static_cast<int>(line.size());
  while (left > 0) {
    const int n = ::send(g_sock, p, left, 0);
    if (n <= 0) {
      return;
    }
    p += n;
    left -= n;
  }
  (void)recv_line(g_sock);
}

void BM_help(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(g_agent->exec_line(":help"));
  }
}
BENCHMARK(BM_help);

void BM_layers(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(g_agent->exec_line(":layers"));
  }
}
BENCHMARK(BM_layers);

void BM_extent(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(g_agent->exec_line(":extent"));
  }
}
BENCHMARK(BM_extent);

void BM_refresh(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(g_agent->exec_line(":refresh"));
  }
}
BENCHMARK(BM_refresh);

void BM_log_level_info(benchmark::State& state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(g_agent->exec_line(":log.level info"));
  }
}
BENCHMARK(BM_log_level_info);

void BM_rpc_ping(benchmark::State& state) {
  const std::string ping =
      "{\"id\":1,\"method\":\"ping\",\"params\":{}}\n";
  for (auto _ : state) {
    roundtrip(ping);
  }
}
BENCHMARK(BM_rpc_ping);

void BM_rpc_extent(benchmark::State& state) {
  const std::string extent_rpc =
      "{\"id\":2,\"method\":\"cmd.exec\",\"params\":{\"line\":\":extent\"}}\n";
  for (auto _ : state) {
    roundtrip(extent_rpc);
  }
}
BENCHMARK(BM_rpc_extent);

void BM_pan(benchmark::State& state) {
  for (auto _ : state) {
    g_map->frame.apply_pan(2, -1);
  }
}
BENCHMARK(BM_pan);

void BM_zoom(benchmark::State& state) {
  for (auto _ : state) {
    g_map->frame.apply_zoom_at(400, 300, 1.01);
  }
}
BENCHMARK(BM_zoom);

void BM_fit_extent(benchmark::State& state) {
  for (auto _ : state) {
    g_map->frame.fit_extent(g_map->scene, 800, 600);
  }
}
BENCHMARK(BM_fit_extent);

void BM_synthetic_feature_load(benchmark::State& state) {
  content::MapScene synthetic;
  synthetic.create_layer("synth", "Point");
  tool::Draft point{};
  point.kind = tool::DraftKind::kPoint;
  point.points = {{1, 2}};
  for (auto _ : state) {
    synthetic.append_from_draft(point, "draw.point");
  }
}
BENCHMARK(BM_synthetic_feature_load);

}  // namespace

int main(int argc, char** argv) {
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);

  ConsoleMap map;
  map.seed();
  content::DebugAgent agent;
  map.bind(&agent);
  g_map = &map;
  g_agent = &agent;

  if (!agent.start()) {
    std::fprintf(stderr, "content_console_bench: DebugAgent start failed\n");
    WSACleanup();
    return 1;
  }
  g_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (g_sock == INVALID_SOCKET) {
    std::fprintf(stderr, "content_console_bench: socket failed\n");
    agent.stop();
    WSACleanup();
    return 1;
  }
  DWORD timeout_ms = 2000;
  ::setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<u_short>(agent.port()));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (::connect(g_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    std::fprintf(stderr, "content_console_bench: connect failed\n");
    closesocket(g_sock);
    agent.stop();
    WSACleanup();
    return 1;
  }

  // Prefer --benchmark_out=… / --benchmark_format=json; SG_CONSOLE_BENCH_JSON
  // still injects --benchmark_out when the flag is absent.
  if (const char* env = std::getenv("SG_CONSOLE_BENCH_JSON")) {
    if (env[0]) {
      bool has_out = false;
      for (int i = 1; i < argc; ++i) {
        if (std::strncmp(argv[i], "--benchmark_out", 15) == 0) {
          has_out = true;
          break;
        }
      }
      if (!has_out) {
        static std::string out_arg;
        out_arg = std::string("--benchmark_out=") + env;
        // Rebuild argv with an extra flag (benchmark copies strings).
        static std::vector<char*> new_argv;
        new_argv.clear();
        new_argv.reserve(static_cast<size_t>(argc) + 2);
        for (int i = 0; i < argc; ++i) {
          new_argv.push_back(argv[i]);
        }
        new_argv.push_back(const_cast<char*>(out_arg.c_str()));
        static char format[] = "--benchmark_format=json";
        new_argv.push_back(format);
        new_argv.push_back(nullptr);
        argc = static_cast<int>(new_argv.size()) - 1;
        argv = new_argv.data();
      }
    }
  }

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    closesocket(g_sock);
    agent.stop();
    WSACleanup();
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();

  closesocket(g_sock);
  agent.stop();
  g_sock = INVALID_SOCKET;
  g_agent = nullptr;
  g_map = nullptr;
  WSACleanup();
  return 0;
}
