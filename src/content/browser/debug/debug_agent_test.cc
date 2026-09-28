// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/debug/debug_agent.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "base/core/log.h"
#include "base/log/log_sink.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

std::string recv_line(SOCKET sock) {
  std::string buf;
  char c;
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

}  // namespace

int main() {
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);

  content::DebugAgent agent;
  assert(agent.start());
  const int port = agent.port();
  assert(port > 0);

  LOGGING(LOG_INFO, "debug_agent_test hello");

  SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(sock != INVALID_SOCKET);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<u_short>(port));
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  assert(::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
         0);

  const char* ping = "{\"id\":1,\"method\":\"ping\",\"params\":{}}\n";
  assert(::send(sock, ping, static_cast<int>(std::strlen(ping)), 0) > 0);
  const std::string ping_resp = recv_line(sock);
  assert(ping_resp.find("\"ok\":true") != std::string::npos);
  assert(ping_resp.find("pong") != std::string::npos);

  const char* tail = "{\"id\":2,\"method\":\"log.tail\",\"params\":{\"n\":50}}\n";
  assert(::send(sock, tail, static_cast<int>(std::strlen(tail)), 0) > 0);
  const std::string tail_resp = recv_line(sock);
  assert(tail_resp.find("\"ok\":true") != std::string::npos);
  assert(tail_resp.find("debug_agent_test hello") != std::string::npos);

  const char* help =
      "{\"id\":3,\"method\":\"cmd.exec\",\"params\":{\"line\":\":help\"}}\n";
  assert(::send(sock, help, static_cast<int>(std::strlen(help)), 0) > 0);
  const std::string help_resp = recv_line(sock);
  assert(help_resp.find(":help") != std::string::npos);

  const char* bad =
      "{\"id\":4,\"method\":\"no.such\",\"params\":{}}\n";
  assert(::send(sock, bad, static_cast<int>(std::strlen(bad)), 0) > 0);
  const std::string bad_resp = recv_line(sock);
  assert(bad_resp.find("\"ok\":false") != std::string::npos);

  const char* ui_unbound =
      "{\"id\":5,\"method\":\"ui.dump_tree\",\"params\":{}}\n";
  assert(::send(sock, ui_unbound, static_cast<int>(std::strlen(ui_unbound)), 0) >
         0);
  const std::string ui_unbound_resp = recv_line(sock);
  assert(ui_unbound_resp.find("\"ok\":false") != std::string::npos);
  assert(ui_unbound_resp.find("ui host not bound") != std::string::npos);

  content::DebugAgentHost host;
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
  agent.set_host(std::move(host));

  const char* ui_tree =
      "{\"id\":6,\"method\":\"ui.dump_tree\",\"params\":{}}\n";
  assert(::send(sock, ui_tree, static_cast<int>(std::strlen(ui_tree)), 0) > 0);
  const std::string ui_tree_resp = recv_line(sock);
  assert(ui_tree_resp.find("\"ok\":true") != std::string::npos);
  assert(ui_tree_resp.find("root") != std::string::npos);

  const char* ui_find =
      "{\"id\":7,\"method\":\"ui.find\",\"params\":{\"name\":\"btn\"}}\n";
  assert(::send(sock, ui_find, static_cast<int>(std::strlen(ui_find)), 0) > 0);
  const std::string ui_find_resp = recv_line(sock);
  assert(ui_find_resp.find("found:btn") != std::string::npos);

  assert(agent.exec_line(":ui tree").find("root") != std::string::npos);
  assert(agent.exec_line(":ui find x").find("found:x") != std::string::npos);
  assert(agent.exec_line(":ui click 3 4").find("click:3,4,1") !=
         std::string::npos);
  assert(agent.exec_line(":ui type hi").find("type:hi") != std::string::npos);
  assert(agent.exec_line(":ui overlay") == "unavailable");

  closesocket(sock);
  agent.stop();

  const std::string local = content::debug_agent().exec_line(":help");
  // Process-wide agent may be idle; exercise instance API instead.
  assert(agent.exec_line(":help").find(":ui") != std::string::npos);
  (void)local;

  std::printf("debug_agent_test OK\n");
  return 0;
}
