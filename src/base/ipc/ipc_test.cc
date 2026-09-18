// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/channel.h"
#include "base/ipc/codec.h"
#include "base/ipc/data_pipe.h"
#include "base/ipc/endpoint.h"
#include "base/ipc/handle.h"
#include "base/ipc/invitation.h"
#include "base/ipc/portal.h"
#include "base/ipc/receiver.h"
#include "content/public/host_protocol.h"

#include <atomic>
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
  static std::atomic<uint32_t> seq{1};
  wchar_t name[96];
  swprintf_s(name, L"\\\\.\\pipe\\smartgis-ipc-test-%u-%lu-%u",
             GetCurrentProcessId(), GetTickCount(), seq.fetch_add(1));
  return name;
}

void test_frame_abi() {
  expect(sizeof(base::ipc::Frame) == 24, "frame 24");
  expect(sizeof(base::ipc::FrameWire) == 24, "frame wire 24");
  expect(sizeof(content::FrameHeader) == 24, "host header 24");
  expect(base::ipc::k_frame_magic == content::kHostMagic, "magic SMT1");
  expect(base::ipc::k_frame_version == content::kHostProtocolVersion,
         "protocol 3");
}

void test_pickle_hello_plugin() {
  content::HelloBody in;
  in.protocol = 3;
  in.role = "gpu";
  in.gpu = "d3d11";
  const std::string wire = base::ipc::encode(in);
  content::HelloBody out;
  expect(base::ipc::decode(wire.data(), wire.size(), &out), "hello decode");
  expect(out.protocol == 3 && out.role == "gpu" && out.gpu == "d3d11",
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
  s_in.present_mode = 1;
  content::SharedHandleWire s_out = {};
  const std::string sw = base::ipc::encode(s_in);
  expect(base::ipc::decode(sw.data(), sw.size(), &s_out), "handle decode");
  expect(s_out.generation == 7 && s_out.width_px == 64 &&
             s_out.present_mode == 1,
         "handle fields");
  const uint64_t planted = 0x1122334455667788ull;
  expect(sw.find(std::string(reinterpret_cast<const char*>(&planted),
                             sizeof(planted))) == std::string::npos,
         "pickle has no nt token");

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
    expect(h.seq != 0, "ack seq");
  });
  expect(server.wait_client(5000), "wait_client");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(server.recv(&h, &payload, 5000), "server recv hello");
  expect(h.magic == base::ipc::k_frame_magic, "recv magic");
  expect(h.version == base::ipc::k_frame_version, "recv version");
  expect(h.flags == base::ipc::k_flag_binary, "recv binary flag");
  expect(h.seq != 0, "hello seq");
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

void test_pipe_rejects_huge_payload() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "huge create");
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
      expect(false, "huge raw connect");
      return;
    }
    base::ipc::FrameWire h = {};
    h.magic = base::ipc::k_frame_magic;
    h.version = base::ipc::k_frame_version;
    h.type = 1;
    h.payload_bytes = base::ipc::k_max_payload_bytes + 1;
    DWORD wrote = 0;
    WriteFile(raw, &h, sizeof(h), &wrote, nullptr);
    CloseHandle(raw);
  });
  expect(server.wait_client(5000), "huge wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(!server.recv(&h, &payload, 5000), "reject huge payload");
  expect(!server.is_open(), "closed after huge payload");
  client_th.join();
}

void test_pipe_half_frame_closes() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "half create");
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
      expect(false, "half raw connect");
      return;
    }
    base::ipc::FrameWire h = {};
    h.magic = base::ipc::k_frame_magic;
    h.version = base::ipc::k_frame_version;
    h.type = 1;
    h.payload_bytes = 16;
    DWORD wrote = 0;
    WriteFile(raw, &h, sizeof(h), &wrote, nullptr);
    const char stub[4] = {1, 2, 3, 4};
    WriteFile(raw, stub, 4, &wrote, nullptr);
    CloseHandle(raw);
  });
  expect(server.wait_client(5000), "half wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(!server.recv(&h, &payload, 5000), "half frame fails");
  expect(!server.is_open(), "closed after half frame");
  client_th.join();
}

void test_handle_transit() {
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "handle create");
  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  expect(ev != nullptr, "create event");
  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "handle connect");
    client.set_peer_process(GetCurrentProcess());
    base::ipc::PlatformHandle local = base::ipc::PlatformHandle::borrow(ev);
    expect(client.send(1, 0, 7, nullptr, 0, &local, 1), "send handle");
  });
  expect(server.wait_client(5000), "handle wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<base::ipc::PlatformHandle> handles;
  expect(server.recv(&h, &payload, &handles, 5000), "recv handle");
  expect(h.seq == 7, "handle seq");
  expect(handles.size() == 1 && handles[0].is_valid(), "one handle");
  expect(SetEvent(ev) != 0, "signal local");
  expect(WaitForSingleObject(handles[0].native, 1000) == WAIT_OBJECT_0,
         "remote wait");
  handles[0].reset();
  client_th.join();
  CloseHandle(ev);
}

void test_invitation_in_process() {
  base::ipc::OutgoingInvitation outgoing;
  base::ipc::Channel parent = outgoing.attach("gpu");
  expect(parent.is_open(), "attach gpu");
  base::ipc::IncomingInvitation incoming;
  expect(outgoing.handoff_local(&incoming), "handoff");
  base::ipc::Channel child = incoming.extract("gpu");
  expect(child.is_open(), "extract gpu");

  content::HelloBody hello;
  hello.role = "gpu";
  expect(parent.send_msg(1, 0, hello), "inv parent send");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  expect(child.recv(&h, &payload, 5000), "inv child recv");
  content::HelloBody out;
  expect(base::ipc::decode(payload.data(), payload.size(), &out),
         "inv decode");
  expect(out.role == "gpu", "inv role");
  expect(child.send_empty(2, 0), "inv child ack");
  expect(parent.recv(&h, &payload, 5000), "inv parent ack");
  expect(h.type == 2, "inv ack type");
}

void test_async_recv() {
  base::ipc::ScopedIpcSupport support;
  expect(base::ipc::ScopedIpcSupport::is_active(), "ipc support");

  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "async create");

  struct Listener : base::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const base::ipc::Frame&,
                    std::vector<uint8_t>,
                    std::vector<base::ipc::PlatformHandle>) override {
      got = 1;
    }
  } listener;

  HANDLE armed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  expect(armed != nullptr, "async armed event");
  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "async connect");
    expect(WaitForSingleObject(armed, 5000) == WAIT_OBJECT_0, "async armed");
    base::ipc::Remote remote(&client);
    const DWORD start = GetTickCount();
    expect(remote.send_empty(9, 1), "async send");
    while (listener.got == 0 && GetTickCount() - start < 5000) {
      Sleep(1);
    }
    expect(listener.got == 1, "async got");
    expect(GetTickCount() - start < 150, "async faster than 200ms poll");
  });
  expect(server.wait_client(5000), "async wait");
  base::ipc::Receiver recv(&server, &listener);
  SetEvent(armed);
  client_th.join();
  CloseHandle(armed);
}

void test_async_recv_dtor() {
  base::ipc::ScopedIpcSupport support;
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "dtor create");

  struct Listener : base::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const base::ipc::Frame&,
                    std::vector<uint8_t>,
                    std::vector<base::ipc::PlatformHandle>) override {
      got.fetch_add(1);
    }
  } listener;

  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "dtor connect");
    base::ipc::Remote remote(&client);
    for (int i = 0; i < 8; ++i) {
      remote.send_empty(3, 0);
      Sleep(5);
    }
  });
  expect(server.wait_client(5000), "dtor wait");
  {
    base::ipc::Receiver recv(&server, &listener);
    Sleep(15);
  }
  client_th.join();
}

void test_data_pipe_local() {
  base::ipc::DataPipeProducer producer;
  base::ipc::DataPipeConsumer consumer;
  expect(base::ipc::create_data_pipe(64, &producer, &consumer), "dp create");
  uint32_t n = 0;
  expect(producer.write("hello", 5, &n) && n == 5, "dp write");
  char buf[8] = {};
  expect(consumer.read(buf, 5, &n) && n == 5, "dp read");
  expect(std::memcmp(buf, "hello", 5) == 0, "dp bytes");
}

void test_data_pipe_transfer() {
  base::ipc::DataPipeProducer producer;
  base::ipc::DataPipeConsumer consumer;
  expect(base::ipc::create_data_pipe(64, &producer, &consumer), "dp2 create");
  uint32_t n = 0;
  expect(producer.write("abcd", 4, &n), "dp2 write");

  base::ipc::OutgoingInvitation outgoing;
  base::ipc::Channel parent = outgoing.attach("dp");
  base::ipc::IncomingInvitation incoming;
  expect(outgoing.handoff_local(&incoming), "dp2 handoff");
  base::ipc::Channel child = incoming.extract("dp");
  expect(parent.is_open() && child.is_open(), "dp2 channels");
  parent.set_peer_process(GetCurrentProcess());

  base::ipc::PlatformHandle exported[base::ipc::k_data_pipe_handles];
  expect(consumer.export_handles(exported), "dp2 export");
  expect(parent.send(1, 0, 0, nullptr, 0, exported, base::ipc::k_data_pipe_handles),
         "dp2 send ends");

  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<base::ipc::PlatformHandle> handles;
  expect(child.recv(&h, &payload, &handles, 5000), "dp2 recv ends");
  expect(handles.size() == base::ipc::k_data_pipe_handles, "dp2 three handles");
  base::ipc::DataPipeConsumer adopted;
  expect(adopted.adopt_handles(handles.data()), "dp2 adopt");
  char buf[8] = {};
  expect(adopted.read(buf, 4, &n) && n == 4, "dp2 read after move");
  expect(std::memcmp(buf, "abcd", 4) == 0, "dp2 moved bytes");
}

void test_portal_local() {
  base::ipc::Portal a;
  base::ipc::Portal b;
  expect(base::ipc::create_local_portal_pair(&a, &b), "portal pair");
  expect(a.send("hi", 2), "portal send");
  std::vector<uint8_t> got;
  expect(b.recv(&got, nullptr, 2000), "portal recv");
  expect(got.size() == 2 && got[0] == 'h' && got[1] == 'i', "portal bytes");
}

void test_portal_transfer() {
  base::ipc::OutgoingInvitation outgoing;
  base::ipc::Channel parent = outgoing.attach("peer");
  base::ipc::IncomingInvitation incoming;
  expect(outgoing.handoff_local(&incoming), "portal handoff");
  base::ipc::Channel child = incoming.extract("peer");
  expect(parent.is_open() && child.is_open(), "portal channels");
  parent.set_peer_process(GetCurrentProcess());
  child.set_peer_process(GetCurrentProcess());

  base::ipc::PlatformChannel local;
  base::ipc::PlatformChannel remote;
  expect(base::ipc::PlatformChannel::create_pair(&local, &remote),
         "portal pair");
  base::ipc::PortalOpenBody body;
  body.keep_id = 1;
  body.move_id = 2;
  base::ipc::PlatformHandle attached =
      base::ipc::PlatformHandle::borrow(remote.get());
  expect(parent.send_msg(base::ipc::k_msg_portal_open, 2, body, &attached, 1),
         "portal offer attach");
  remote.close();

  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<base::ipc::PlatformHandle> handles;
  expect(child.recv(&h, &payload, &handles, 5000), "portal take recv");
  expect(h.type == base::ipc::k_msg_portal_open, "portal open type");
  expect(handles.size() == 1 && handles[0].is_valid(), "portal pipe handle");

  base::ipc::Channel keep;
  base::ipc::Channel taken;
  expect(keep.adopt(local.release()), "portal keep adopt");
  expect(taken.adopt(handles[0].release()), "portal take adopt");
  expect(keep.send(base::ipc::k_msg_portal_bytes, 0, "ping", 4),
         "portal remote send");
  expect(taken.recv(&h, &payload, 5000), "portal remote recv");
  expect(payload.size() == 4 && std::memcmp(payload.data(), "ping", 4) == 0,
         "portal transferred bytes");
}

void test_pending_endpoint() {
  base::ipc::PendingRemote pending_remote;
  base::ipc::PendingReceiver pending_receiver;
  expect(base::ipc::InterfaceEndpoint::create_pair(&pending_remote,
                                                   &pending_receiver),
         "pending pair");

  struct Listener : base::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const base::ipc::Frame& frame,
                    std::vector<uint8_t>,
                    std::vector<base::ipc::PlatformHandle>) override {
      if (frame.type == 11) {
        got = 1;
      }
    }
  } listener;

  base::ipc::Receiver receiver(std::move(pending_receiver), &listener);
  base::ipc::Remote remote(std::move(pending_remote));
  expect(remote.send_empty(11, 0), "pending send");
  const DWORD start = GetTickCount();
  while (listener.got == 0 && GetTickCount() - start < 5000) {
    Sleep(10);
  }
  expect(listener.got == 1, "pending got");
}

void test_shared_handle_attachment_only() {
  content::SharedHandleWire w = {};
  w.generation = 9;
  w.width_px = 8;
  w.height_px = 8;
  w.format = content::kDxgiBgraUnorm;
  w.present_mode = 1;

  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  expect(ev != nullptr, "attach event");
  const std::wstring name = make_pipe_path();
  base::ipc::Channel server;
  expect(server.create_server(name), "attach create");
  std::thread client_th([&] {
    base::ipc::Channel client;
    expect(client.connect_client(name, 5000), "attach connect");
    client.set_peer_process(GetCurrentProcess());
    base::ipc::PlatformHandle h = base::ipc::PlatformHandle::borrow(ev);
    expect(client.send_msg(
               static_cast<uint16_t>(content::HostMsg::kSharedHandle), 1, w, &h,
               1),
           "attach send");
  });
  expect(server.wait_client(5000), "attach wait");
  base::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<base::ipc::PlatformHandle> handles;
  expect(server.recv(&h, &payload, &handles, 5000), "attach recv");
  expect(h.handle_count == 1 && handles.size() == 1 && handles[0].is_valid(),
         "attach one handle");
  content::SharedHandleWire out = {};
  expect(base::ipc::decode(payload.data(), payload.size(), &out),
         "attach decode");
  expect(out.generation == 9 && out.width_px == 8, "attach meta");
  expect(SetEvent(ev) != 0, "attach signal");
  expect(WaitForSingleObject(handles[0].native, 1000) == WAIT_OBJECT_0,
         "attach wait event");
  client_th.join();
  CloseHandle(ev);
}

}  // namespace

void run(const char* name, void (*fn)()) {
  std::fprintf(stdout, "%s\n", name);
  std::fflush(stdout);
  fn();
}

int main() {
  run("frame_abi", test_frame_abi);
  run("pickle_hello_plugin", test_pickle_hello_plugin);
  run("pickle_host_bodies", test_pickle_host_bodies);
  run("decode_rejects_bad_input", test_decode_rejects_bad_input);
  run("pipe_roundtrip", test_pipe_roundtrip);
  run("pipe_empty_and_view_id", test_pipe_empty_and_view_id);
  run("pipe_recv_timeout", test_pipe_recv_timeout);
  run("pipe_rejects_bad_magic", test_pipe_rejects_bad_magic);
  run("pipe_rejects_huge_payload", test_pipe_rejects_huge_payload);
  run("pipe_half_frame_closes", test_pipe_half_frame_closes);
  run("handle_transit", test_handle_transit);
  run("invitation_in_process", test_invitation_in_process);
  run("async_recv", test_async_recv);
  run("async_recv_dtor", test_async_recv_dtor);
  run("data_pipe_local", test_data_pipe_local);
  run("data_pipe_transfer", test_data_pipe_transfer);
  run("portal_local", test_portal_local);
  run("portal_transfer", test_portal_transfer);
  run("pending_endpoint", test_pending_endpoint);
  run("shared_handle_attachment_only", test_shared_handle_attachment_only);
  if (g_fails) {
    std::fprintf(stderr, "ipc_test: %d FAIL\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "ipc_test: ok\n");
  return 0;
}
