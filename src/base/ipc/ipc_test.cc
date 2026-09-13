// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/channel.h"
#include "base/ipc/codec.h"
#include "content/public/host_protocol.h"

#include <cstdio>
#include <cstring>
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

std::wstring make_pipe_path() {
  wchar_t name[96];
  swprintf_s(name, L"\\\\.\\pipe\\smartgis-ipc-test-%u-%lu",
             GetCurrentProcessId(), GetTickCount());
  return name;
}

void test_frame_abi() {
  expect(sizeof(base::ipc::FrameWire) == 18, "frame wire 18");
  expect(sizeof(content::FrameHeader) == 18, "host header 18");
  expect(base::ipc::k_frame_magic == content::kHostMagic, "magic SMT1");
  expect(base::ipc::k_frame_version == content::kHostProtocolVersion,
         "protocol 2");
}

void test_pickle_hello_plugin() {
  content::HelloBody in;
  in.protocol = 2;
  in.role = "gpu";
  in.gpu = "d3d11";
  const std::string wire = base::ipc::encode(in);
  content::HelloBody out;
  expect(base::ipc::decode(wire.data(), wire.size(), &out), "hello decode");
  expect(out.protocol == 2 && out.role == "gpu" && out.gpu == "d3d11",
         "hello fields");

  content::PluginCallBody p;
  p.plugin_id = "orthogrid";
  p.method = "run";
  p.bytes = std::string("\x00\x01", 2);
  const std::string pw = base::ipc::encode(p);
  content::PluginCallBody q;
  expect(base::ipc::decode(pw.data(), pw.size(), &q), "plugin decode");
  expect(q.plugin_id == "orthogrid" && q.method == "run" && q.bytes == p.bytes,
         "plugin fields");
}

void test_pickle_host_bodies() {
  content::ExtentWire e_in;
  e_in.xmin = -1.5;
  e_in.ymin = 2.25;
  e_in.xmax = 10;
  e_in.ymax = 20.5;
  content::ExtentWire e_out;
  const std::string ew = base::ipc::encode(e_in);
  expect(base::ipc::decode(ew.data(), ew.size(), &e_out), "extent decode");
  expect(e_out.xmin == e_in.xmin && e_out.ymax == e_in.ymax, "extent fields");

  content::SharedHandleWire s_in = {};
  s_in.generation = 7;
  s_in.width_px = 64;
  s_in.height_px = 48;
  s_in.format = content::kDxgiBgraUnorm;
  s_in.nt_handle = 0x1122334455667788ull;
  s_in.present_mode = 1;
  content::SharedHandleWire s_out = {};
  const std::string sw = base::ipc::encode(s_in);
  expect(base::ipc::decode(sw.data(), sw.size(), &s_out), "handle decode");
  expect(s_out.generation == 7 && s_out.nt_handle == s_in.nt_handle,
         "handle fields");

  content::PointerEventWire p_in = {};
  p_in.t_qpc = 99;
  p_in.kind = 3;
  p_in.x_px = -4;
  p_in.y_px = 8;
  p_in.dpi = 96.f;
  content::PointerEventWire p_out = {};
  const std::string pw = base::ipc::encode(p_in);
  expect(base::ipc::decode(pw.data(), pw.size(), &p_out), "pointer decode");
  expect(p_out.t_qpc == 99 && p_out.x_px == -4 && p_out.dpi == 96.f,
         "pointer fields");

  content::ResizeSurfaceBody r_in;
  r_in.w = 1280;
  r_in.h = 720;
  r_in.dpi = 144.f;
  content::ResizeSurfaceBody r_out;
  const std::string rw = base::ipc::encode(r_in);
  expect(base::ipc::decode(rw.data(), rw.size(), &r_out), "resize decode");
  expect(r_out.w == 1280 && r_out.h == 720 && r_out.dpi == 144.f,
         "resize fields");
}

void test_decode_rejects_bad_input() {
  content::HelloBody out;
  expect(!base::ipc::decode(nullptr, 8, &out), "null data");
  expect(!base::ipc::decode("", 0, static_cast<content::HelloBody*>(nullptr)),
         "null out");

  const char trunc[] = "\x02\x00";
  content::HelloBody truncated;
  truncated.role = "keep";
  expect(!base::ipc::decode(trunc, 2, &truncated), "truncated hello");
}

void test_pipe_roundtrip() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "create_server");
  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "connect_client");
    content::HelloBody hello;
    hello.role = "gpu";
    hello.gpu = "d3d11";
    expect(client.send_msg(static_cast<uint16_t>(content::HostMsg::kHello), 0,
                             hello),
           "client send hello");
    base::ipc::Frame h;
    std::vector<uint8_t> payload;
    expect(client.recv(&h, &payload, 5000), "client recv ack");
    expect(h.type == static_cast<uint16_t>(content::HostMsg::kHelloAck),
           "ack type");
    expect(h.payload_bytes == 0, "ack empty");
  });
  expect(server.wait_client(5000), "wait_client");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(server.recv(&h, &payload, 5000), "server recv hello");
  expect(h.magic == base::ipc::k_frame_magic, "recv magic");
  expect(h.version == base::ipc::k_frame_version, "recv version");
  expect(h.flags == base::ipc::k_flag_binary, "recv binary flag");
  content::HelloBody hello;
  expect(base::ipc::decode(payload.data(), payload.size(), &hello),
         "server decode");
  expect(hello.role == "gpu", "server hello role");
  expect(server.send_empty(static_cast<uint16_t>(content::HostMsg::kHelloAck),
                            0),
         "server ack");
  client_th.join();
}

void test_pipe_empty_and_view_id() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "empty create");
  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "empty connect");
    expect(client.send_empty(
               static_cast<uint16_t>(content::HostMsg::kShutdown), 42),
           "send shutdown");
  });
  expect(server.wait_client(5000), "empty wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(server.recv(&h, &payload, 5000), "recv shutdown");
  expect(h.type == static_cast<uint16_t>(content::HostMsg::kShutdown),
         "shutdown type");
  expect(h.view_id == 42, "view id");
  expect(payload.empty() && h.payload_bytes == 0, "empty payload");
  client_th.join();
}

void test_pipe_recv_timeout() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "timeout create");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(!server.recv(&h, &payload, 80), "recv timeout");
  expect(server.is_open(), "still open after timeout");
}

void test_pipe_rejects_bad_magic() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "bad-magic create");
  std::thread client_th([&] {
    HANDLE raw = INVALID_HANDLE_VALUE;
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < 5000) {
      raw = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                        OPEN_EXISTING, 0, nullptr);
      if (raw != INVALID_HANDLE_VALUE) {
        break;
      }
      Sleep(20);
    }
    if (raw == INVALID_HANDLE_VALUE) {
      expect(false, "raw connect");
      return;
    }
    base::ipc::FrameWire bad = {};
    DWORD wrote = 0;
    WriteFile(raw, &bad, sizeof(bad), &wrote, nullptr);
    CloseHandle(raw);
  });
  expect(server.wait_client(5000), "bad-magic wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(!server.recv(&h, &payload, 5000), "reject bad magic");
  expect(!server.is_open(), "closed after bad magic");
  client_th.join();
}

}  // namespace

int main() {
  test_frame_abi();
  test_pickle_hello_plugin();
  test_pickle_host_bodies();
  test_decode_rejects_bad_input();
  test_pipe_roundtrip();
  test_pipe_empty_and_view_id();
  test_pipe_recv_timeout();
  test_pipe_rejects_bad_magic();
  if (g_fails) {
    std::fprintf(stderr, "ipc_test: %d FAIL\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "ipc_test: ok\n");
  return 0;
}
