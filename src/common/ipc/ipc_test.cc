// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "common/ipc/broker/broker.h"
#include "common/ipc/bulk/bulk.h"
#include "common/ipc/channel/channel.h"
#include "common/ipc/codec/codec.h"
#include "common/ipc/data_pipe/data_pipe.h"
#include "common/ipc/endpoint/endpoint.h"
#include "common/ipc/handle/handle.h"
#include "common/ipc/invitation/invitation.h"
#include "common/ipc/portal/portal.h"
#include "common/ipc/proxy/channel_proxy.h"
#include "common/ipc/receiver/receiver.h"
#include "content/common/host_msg_registry.h"
#include "content/common/host_protocol.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>


namespace {

std::wstring make_pipe_path() {
  static std::atomic<uint32_t> seq{1};
  wchar_t name[96];
  swprintf_s(name, L"\\\\.\\pipe\\smartgis-ipc-test-%u-%lu-%u",
             GetCurrentProcessId(), GetTickCount(), seq.fetch_add(1));
  return name;
}

}  // namespace



TEST(Ipc, FrameAbi) {
  EXPECT_TRUE(sizeof(common::ipc::Frame) == 24) << "frame 24";
  EXPECT_TRUE(sizeof(common::ipc::FrameWire) == 24) << "frame wire 24";
  EXPECT_TRUE(sizeof(content::FrameHeader) == 24) << "host header 24";
  EXPECT_TRUE(common::ipc::k_frame_magic == content::kHostMagic) << "magic GIS1";
  EXPECT_TRUE(common::ipc::k_frame_version == content::kHostProtocolVersion) << "protocol 3";
}

TEST(Ipc, PickleHelloPlugin) {
  content::HelloBody in;
  in.protocol = 3;
  in.role = "gpu";
  in.gpu = "d3d11";
  const std::string wire = common::ipc::encode(in);
  content::HelloBody out;
  EXPECT_TRUE(common::ipc::decode(wire.data(), wire.size(), &out)) << "hello decode";
  EXPECT_TRUE(out.protocol == 3 && out.role == "gpu" && out.gpu == "d3d11") << "hello fields";

  content::PluginCallBody p;
  p.plugin_id = "orthogrid";
  p.method = "run";
  p.bytes = std::string("\x00\x01", 2);
  const std::string pw = common::ipc::encode(p);
  content::PluginCallBody q;
  EXPECT_TRUE(common::ipc::decode(pw.data(), pw.size(), &q)) << "plugin decode";
  EXPECT_TRUE(q.plugin_id == "orthogrid" && q.method == "run" && q.bytes == p.bytes) << "plugin fields";
}

TEST(Ipc, PickleHostBodies) {
  content::ExtentWire e_in;
  e_in.xmin = -1.5;
  e_in.ymin = 2.25;
  e_in.xmax = 10;
  e_in.ymax = 20.5;
  content::ExtentWire e_out;
  const std::string ew = common::ipc::encode(e_in);
  EXPECT_TRUE(common::ipc::decode(ew.data(), ew.size(), &e_out)) << "extent decode";
  EXPECT_TRUE(e_out.xmin == e_in.xmin && e_out.ymax == e_in.ymax) << "extent fields";

  content::SharedHandleWire s_in = {};
  s_in.generation = 7;
  s_in.width_px = 64;
  s_in.height_px = 48;
  s_in.format = content::kDxgiBgraUnorm;
  s_in.present_mode = 1;
  content::SharedHandleWire s_out = {};
  const std::string sw = common::ipc::encode(s_in);
  EXPECT_TRUE(common::ipc::decode(sw.data(), sw.size(), &s_out)) << "handle decode";
  EXPECT_TRUE(s_out.generation == 7 && s_out.width_px == 64 &&
             s_out.present_mode == 1) << "handle fields";
  const uint64_t planted = 0x1122334455667788ull;
  EXPECT_TRUE(sw.find(std::string(reinterpret_cast<const char*>(&planted),
                             sizeof(planted))) == std::string::npos) << "pickle has no nt token";

  content::PointerEventWire p_in = {};
  p_in.t_qpc = 99;
  p_in.kind = 3;
  p_in.x_px = -4;
  p_in.y_px = 8;
  p_in.dpi = 96.f;
  p_in.pointer_count = 2;
  content::PointerEventWire p_out = {};
  const std::string pw = common::ipc::encode(p_in);
  EXPECT_TRUE(common::ipc::decode(pw.data(), pw.size(), &p_out)) << "pointer decode";
  EXPECT_TRUE(p_out.t_qpc == 99 && p_out.x_px == -4 && p_out.dpi == 96.f &&
             p_out.pointer_count == 2) << "pointer fields";

  content::ResizeSurfaceBody r_in;
  r_in.w = 1280;
  r_in.h = 720;
  r_in.dpi = 144.f;
  r_in.monitor_luid_low = 0xabcdef01u;
  r_in.monitor_luid_high = 0x12345678u;
  r_in.adapter_hint = 2;
  content::ResizeSurfaceBody r_out;
  const std::string rw = common::ipc::encode(r_in);
  EXPECT_TRUE(common::ipc::decode(rw.data(), rw.size(), &r_out)) << "resize decode";
  EXPECT_TRUE(r_out.w == 1280 && r_out.h == 720 && r_out.dpi == 144.f &&
             r_out.monitor_luid_low == 0xabcdef01u &&
             r_out.monitor_luid_high == 0x12345678u &&
             r_out.adapter_hint == 2) << "resize fields";
}

TEST(Ipc, DecodeRejectsBadInput) {
  content::HelloBody out;
  EXPECT_TRUE(!common::ipc::decode(nullptr, 8, &out)) << "null data";
  EXPECT_TRUE(!common::ipc::decode("", 0, static_cast<content::HelloBody*>(nullptr))) << "null out";

  const char trunc[] = "\x02\x00";
  content::HelloBody truncated;
  truncated.role = "keep";
  EXPECT_TRUE(!common::ipc::decode(trunc, 2, &truncated)) << "truncated hello";
}

TEST(Ipc, PipeRoundtrip) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "create_server";
  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "connect_client";
    content::HelloBody hello;
    hello.role = "gpu";
    hello.gpu = "d3d11";
    EXPECT_TRUE(client.send_msg(static_cast<uint16_t>(content::HostMsg::kHello), 0,
                             hello)) << "client send hello";
    common::ipc::Frame h;
    std::vector<uint8_t> payload;
    EXPECT_TRUE(client.recv(&h, &payload, 5000)) << "client recv ack";
    EXPECT_TRUE(h.type == static_cast<uint16_t>(content::HostMsg::kHelloAck)) << "ack type";
    EXPECT_TRUE(h.payload_bytes == 0) << "ack empty";
    EXPECT_TRUE(h.seq != 0) << "ack seq";
  });
  EXPECT_TRUE(server.wait_client(5000)) << "wait_client";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(server.recv(&h, &payload, 5000)) << "server recv hello";
  EXPECT_TRUE(h.magic == common::ipc::k_frame_magic) << "recv magic";
  EXPECT_TRUE(h.version == common::ipc::k_frame_version) << "recv version";
  EXPECT_TRUE(h.flags == common::ipc::k_flag_binary) << "recv binary flag";
  EXPECT_TRUE(h.seq != 0) << "hello seq";
  content::HelloBody hello;
  EXPECT_TRUE(common::ipc::decode(payload.data(), payload.size(), &hello)) << "server decode";
  EXPECT_TRUE(hello.role == "gpu") << "server hello role";
  EXPECT_TRUE(server.send_empty(static_cast<uint16_t>(content::HostMsg::kHelloAck),
                            0)) << "server ack";
  client_th.join();
}

TEST(Ipc, PipeEmptyAndViewId) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "empty create";
  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "empty connect";
    EXPECT_TRUE(client.send_empty(
               static_cast<uint16_t>(content::HostMsg::kShutdown), 42)) << "send shutdown";
  });
  EXPECT_TRUE(server.wait_client(5000)) << "empty wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(server.recv(&h, &payload, 5000)) << "recv shutdown";
  EXPECT_TRUE(h.type == static_cast<uint16_t>(content::HostMsg::kShutdown)) << "shutdown type";
  EXPECT_TRUE(h.view_id == 42) << "view id";
  EXPECT_TRUE(payload.empty() && h.payload_bytes == 0) << "empty payload";
  client_th.join();
}

TEST(Ipc, PipeRecvTimeout) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "timeout create";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(!server.recv(&h, &payload, 80)) << "recv timeout";
  EXPECT_TRUE(server.is_open()) << "still open after timeout";
}

TEST(Ipc, PipeRejectsBadMagic) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "bad-magic create";
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
      EXPECT_TRUE(false) << "raw connect";
      return;
    }
    common::ipc::FrameWire bad = {};
    DWORD wrote = 0;
    WriteFile(raw, &bad, sizeof(bad), &wrote, nullptr);
    CloseHandle(raw);
  });
  EXPECT_TRUE(server.wait_client(5000)) << "bad-magic wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(!server.recv(&h, &payload, 5000)) << "reject bad magic";
  EXPECT_TRUE(!server.is_open()) << "closed after bad magic";
  client_th.join();
}

TEST(Ipc, PipeRejectsHugePayload) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "huge create";
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
      EXPECT_TRUE(false) << "huge raw connect";
      return;
    }
    common::ipc::FrameWire h = {};
    h.magic = common::ipc::k_frame_magic;
    h.version = common::ipc::k_frame_version;
    h.type = 1;
    h.payload_bytes = common::ipc::k_max_payload_bytes + 1;
    DWORD wrote = 0;
    WriteFile(raw, &h, sizeof(h), &wrote, nullptr);
    CloseHandle(raw);
  });
  EXPECT_TRUE(server.wait_client(5000)) << "huge wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(!server.recv(&h, &payload, 5000)) << "reject huge payload";
  EXPECT_TRUE(!server.is_open()) << "closed after huge payload";
  client_th.join();
}

TEST(Ipc, PipeHalfFrameCloses) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "half create";
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
      EXPECT_TRUE(false) << "half raw connect";
      return;
    }
    common::ipc::FrameWire h = {};
    h.magic = common::ipc::k_frame_magic;
    h.version = common::ipc::k_frame_version;
    h.type = 1;
    h.payload_bytes = 16;
    DWORD wrote = 0;
    WriteFile(raw, &h, sizeof(h), &wrote, nullptr);
    const char stub[4] = {1, 2, 3, 4};
    WriteFile(raw, stub, 4, &wrote, nullptr);
    CloseHandle(raw);
  });
  EXPECT_TRUE(server.wait_client(5000)) << "half wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(!server.recv(&h, &payload, 5000)) << "half frame fails";
  EXPECT_TRUE(!server.is_open()) << "closed after half frame";
  client_th.join();
}

TEST(Ipc, HandleTransit) {
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "handle create";
  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  EXPECT_TRUE(ev != nullptr) << "create event";
  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "handle connect";
    client.set_peer_process(GetCurrentProcess());
    common::ipc::PlatformHandle local = common::ipc::PlatformHandle::borrow(ev);
    EXPECT_TRUE(client.send(1, 0, 7, nullptr, 0, &local, 1)) << "send handle";
  });
  EXPECT_TRUE(server.wait_client(5000)) << "handle wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<common::ipc::PlatformHandle> handles;
  EXPECT_TRUE(server.recv(&h, &payload, &handles, 5000)) << "recv handle";
  EXPECT_TRUE(h.seq == 7) << "handle seq";
  EXPECT_TRUE(handles.size() == 1 && handles[0].is_valid()) << "one handle";
  EXPECT_TRUE(SetEvent(ev) != 0) << "signal local";
  EXPECT_TRUE(WaitForSingleObject(handles[0].native, 1000) == WAIT_OBJECT_0) << "remote wait";
  handles[0].reset();
  client_th.join();
  CloseHandle(ev);
}

TEST(Ipc, InvitationInProcess) {
  common::ipc::OutgoingInvitation outgoing;
  common::ipc::Channel parent = outgoing.attach("gpu");
  EXPECT_TRUE(parent.is_open()) << "attach gpu";
  common::ipc::IncomingInvitation incoming;
  EXPECT_TRUE(outgoing.handoff_local(&incoming)) << "handoff";
  common::ipc::Channel child = incoming.extract("gpu");
  EXPECT_TRUE(child.is_open()) << "extract gpu";

  content::HelloBody hello;
  hello.role = "gpu";
  EXPECT_TRUE(parent.send_msg(1, 0, hello)) << "inv parent send";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  EXPECT_TRUE(child.recv(&h, &payload, 5000)) << "inv child recv";
  content::HelloBody out;
  EXPECT_TRUE(common::ipc::decode(payload.data(), payload.size(), &out)) << "inv decode";
  EXPECT_TRUE(out.role == "gpu") << "inv role";
  EXPECT_TRUE(child.send_empty(2, 0)) << "inv child ack";
  EXPECT_TRUE(parent.recv(&h, &payload, 5000)) << "inv parent ack";
  EXPECT_TRUE(h.type == 2) << "inv ack type";
}

TEST(Ipc, AsyncRecv) {
  common::ipc::ScopedIpcSupport support;
  EXPECT_TRUE(common::ipc::ScopedIpcSupport::is_active()) << "ipc support";

  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "async create";

  struct Listener : common::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const common::ipc::Frame&,
                    std::vector<uint8_t>,
                    std::vector<common::ipc::PlatformHandle>) override {
      got = 1;
    }
  } listener;

  HANDLE armed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  EXPECT_TRUE(armed != nullptr) << "async armed event";
  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "async connect";
    EXPECT_TRUE(WaitForSingleObject(armed, 5000) == WAIT_OBJECT_0) << "async armed";
    common::ipc::Remote remote(&client);
    const DWORD start = GetTickCount();
    EXPECT_TRUE(remote.send_empty(9, 1)) << "async send";
    while (listener.got == 0 && GetTickCount() - start < 5000) {
      Sleep(1);
    }
    EXPECT_TRUE(listener.got == 1) << "async got";
    EXPECT_TRUE(GetTickCount() - start < 150) << "async faster than 200ms poll";
  });
  EXPECT_TRUE(server.wait_client(5000)) << "async wait";
  common::ipc::Receiver recv(&server, &listener);
  SetEvent(armed);
  client_th.join();
  CloseHandle(armed);
}

TEST(Ipc, AsyncRecvDtor) {
  common::ipc::ScopedIpcSupport support;
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "dtor create";

  struct Listener : common::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const common::ipc::Frame&,
                    std::vector<uint8_t>,
                    std::vector<common::ipc::PlatformHandle>) override {
      got.fetch_add(1);
    }
  } listener;

  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "dtor connect";
    common::ipc::Remote remote(&client);
    for (int i = 0; i < 8; ++i) {
      remote.send_empty(3, 0);
      Sleep(5);
    }
  });
  EXPECT_TRUE(server.wait_client(5000)) << "dtor wait";
  {
    common::ipc::Receiver recv(&server, &listener);
    Sleep(15);
  }
  client_th.join();
}

TEST(Ipc, DataPipeLocal) {
  common::ipc::DataPipeProducer producer;
  common::ipc::DataPipeConsumer consumer;
  EXPECT_TRUE(common::ipc::create_data_pipe(64, &producer, &consumer)) << "dp create";
  uint32_t n = 0;
  EXPECT_TRUE(producer.write("hello", 5, &n) && n == 5) << "dp write";
  char buf[8] = {};
  EXPECT_TRUE(consumer.read(buf, 5, &n) && n == 5) << "dp read";
  EXPECT_TRUE(std::memcmp(buf, "hello", 5) == 0) << "dp bytes";
}

TEST(Ipc, DataPipeTransfer) {
  common::ipc::DataPipeProducer producer;
  common::ipc::DataPipeConsumer consumer;
  EXPECT_TRUE(common::ipc::create_data_pipe(64, &producer, &consumer)) << "dp2 create";
  uint32_t n = 0;
  EXPECT_TRUE(producer.write("abcd", 4, &n)) << "dp2 write";

  common::ipc::OutgoingInvitation outgoing;
  common::ipc::Channel parent = outgoing.attach("dp");
  common::ipc::IncomingInvitation incoming;
  EXPECT_TRUE(outgoing.handoff_local(&incoming)) << "dp2 handoff";
  common::ipc::Channel child = incoming.extract("dp");
  EXPECT_TRUE(parent.is_open() && child.is_open()) << "dp2 channels";
  parent.set_peer_process(GetCurrentProcess());

  common::ipc::PlatformHandle exported[common::ipc::k_data_pipe_handles];
  EXPECT_TRUE(consumer.export_handles(exported)) << "dp2 export";
  EXPECT_TRUE(parent.send(1, 0, 0, nullptr, 0, exported, common::ipc::k_data_pipe_handles)) << "dp2 send ends";

  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<common::ipc::PlatformHandle> handles;
  EXPECT_TRUE(child.recv(&h, &payload, &handles, 5000)) << "dp2 recv ends";
  EXPECT_TRUE(handles.size() == common::ipc::k_data_pipe_handles) << "dp2 three handles";
  common::ipc::DataPipeConsumer adopted;
  EXPECT_TRUE(adopted.adopt_handles(handles.data())) << "dp2 adopt";
  char buf[8] = {};
  EXPECT_TRUE(adopted.read(buf, 4, &n) && n == 4) << "dp2 read after move";
  EXPECT_TRUE(std::memcmp(buf, "abcd", 4) == 0) << "dp2 moved bytes";
}

TEST(Ipc, PortalLocal) {
  common::ipc::Portal a;
  common::ipc::Portal b;
  EXPECT_TRUE(common::ipc::create_local_portal_pair(&a, &b)) << "portal pair";
  EXPECT_TRUE(a.send("hi", 2)) << "portal send";
  std::vector<uint8_t> got;
  EXPECT_TRUE(b.recv(&got, nullptr, 2000)) << "portal recv";
  EXPECT_TRUE(got.size() == 2 && got[0] == 'h' && got[1] == 'i') << "portal bytes";
}

TEST(Ipc, PortalTransfer) {
  common::ipc::OutgoingInvitation outgoing;
  common::ipc::Channel parent = outgoing.attach("peer");
  common::ipc::IncomingInvitation incoming;
  EXPECT_TRUE(outgoing.handoff_local(&incoming)) << "portal handoff";
  common::ipc::Channel child = incoming.extract("peer");
  EXPECT_TRUE(parent.is_open() && child.is_open()) << "portal channels";
  parent.set_peer_process(GetCurrentProcess());
  child.set_peer_process(GetCurrentProcess());

  common::ipc::PlatformChannel local;
  common::ipc::PlatformChannel remote;
  EXPECT_TRUE(common::ipc::PlatformChannel::create_pair(&local, &remote)) << "portal pair";
  common::ipc::PortalOpenBody body;
  body.keep_id = 1;
  body.move_id = 2;
  common::ipc::PlatformHandle attached =
      common::ipc::PlatformHandle::borrow(remote.get());
  EXPECT_TRUE(parent.send_msg(common::ipc::k_msg_portal_open, 2, body, &attached, 1)) << "portal offer attach";
  remote.close();

  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<common::ipc::PlatformHandle> handles;
  EXPECT_TRUE(child.recv(&h, &payload, &handles, 5000)) << "portal take recv";
  EXPECT_TRUE(h.type == common::ipc::k_msg_portal_open) << "portal open type";
  EXPECT_TRUE(handles.size() == 1 && handles[0].is_valid()) << "portal pipe handle";

  common::ipc::Channel keep;
  common::ipc::Channel taken;
  EXPECT_TRUE(keep.adopt(local.release())) << "portal keep adopt";
  EXPECT_TRUE(taken.adopt(handles[0].release())) << "portal take adopt";
  EXPECT_TRUE(keep.send(common::ipc::k_msg_portal_bytes, 0, "ping", 4)) << "portal remote send";
  EXPECT_TRUE(taken.recv(&h, &payload, 5000)) << "portal remote recv";
  EXPECT_TRUE(payload.size() == 4 && std::memcmp(payload.data(), "ping", 4) == 0) << "portal transferred bytes";
}

TEST(Ipc, PendingEndpoint) {
  common::ipc::PendingRemote pending_remote;
  common::ipc::PendingReceiver pending_receiver;
  EXPECT_TRUE(common::ipc::InterfaceEndpoint::create_pair(&pending_remote,
                                                   &pending_receiver)) << "pending pair";

  struct Listener : common::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const common::ipc::Frame& frame,
                    std::vector<uint8_t>,
                    std::vector<common::ipc::PlatformHandle>) override {
      if (frame.type == 11) {
        got = 1;
      }
    }
  } listener;

  common::ipc::Receiver receiver(std::move(pending_receiver), &listener);
  common::ipc::Remote remote(std::move(pending_remote));
  EXPECT_TRUE(remote.send_empty(11, 0)) << "pending send";
  const DWORD start = GetTickCount();
  while (listener.got == 0 && GetTickCount() - start < 5000) {
    Sleep(10);
  }
  EXPECT_TRUE(listener.got == 1) << "pending got";
}

TEST(Ipc, SharedHandleAttachmentOnly) {
  content::SharedHandleWire w = {};
  w.generation = 9;
  w.width_px = 8;
  w.height_px = 8;
  w.format = content::kDxgiBgraUnorm;
  w.present_mode = 1;

  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  EXPECT_TRUE(ev != nullptr) << "attach event";
  const std::wstring name = make_pipe_path();
  common::ipc::Channel server;
  EXPECT_TRUE(server.create_server(name)) << "attach create";
  std::thread client_th([&] {
    common::ipc::Channel client;
    EXPECT_TRUE(client.connect_client(name, 5000)) << "attach connect";
    client.set_peer_process(GetCurrentProcess());
    common::ipc::PlatformHandle h = common::ipc::PlatformHandle::borrow(ev);
    EXPECT_TRUE(client.send_msg(
               static_cast<uint16_t>(content::HostMsg::kSharedHandle), 1, w, &h,
               1)) << "attach send";
  });
  EXPECT_TRUE(server.wait_client(5000)) << "attach wait";
  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<common::ipc::PlatformHandle> handles;
  EXPECT_TRUE(server.recv(&h, &payload, &handles, 5000)) << "attach recv";
  EXPECT_TRUE(h.handle_count == 1 && handles.size() == 1 && handles[0].is_valid()) << "attach one handle";
  content::SharedHandleWire out = {};
  EXPECT_TRUE(common::ipc::decode(payload.data(), payload.size(), &out)) << "attach decode";
  EXPECT_TRUE(out.generation == 9 && out.width_px == 8) << "attach meta";
  EXPECT_TRUE(SetEvent(ev) != 0) << "attach signal";
  EXPECT_TRUE(WaitForSingleObject(handles[0].native, 1000) == WAIT_OBJECT_0) << "attach wait event";
  client_th.join();
  CloseHandle(ev);
}

TEST(Ipc, ChannelProxySyncSend) {
  common::ipc::PendingRemote remote;
  common::ipc::PendingReceiver pending;
  EXPECT_TRUE(common::ipc::InterfaceEndpoint::create_pair(&remote, &pending)) << "proxy pair";
  struct Listener final : common::ipc::MessageListener {
    std::atomic<int> got{0};
    void on_message(const common::ipc::Frame& frame,
                    std::vector<uint8_t> payload,
                    std::vector<common::ipc::PlatformHandle> handles) override {
      (void)payload;
      (void)handles;
      if (frame.type == static_cast<uint16_t>(content::HostMsg::kHelloAck) &&
          frame.view_id == 7) {
        got.fetch_add(1);
      }
    }
  } listener;
  // Listen on the receiver end; send from the remote end via ChannelProxy.
  common::ipc::ChannelProxy peer(pending.take_channel());
  EXPECT_TRUE(peer.bind(&listener)) << "proxy bind";
  common::ipc::ChannelProxy sender(remote.take_channel());
  EXPECT_TRUE(sender.send_empty(static_cast<uint16_t>(content::HostMsg::kHelloAck),
                           7)) << "proxy send";
  const DWORD start = GetTickCount();
  while (listener.got.load() == 0 && GetTickCount() - start < 5000) {
    Sleep(10);
  }
  EXPECT_TRUE(listener.got.load() == 1) << "proxy got ack";
  sender.reset();
  peer.reset();
}

TEST(Ipc, HostMsgRegistry) {
  EXPECT_TRUE(content::is_known_host_msg(
             static_cast<uint16_t>(content::HostMsg::kFrameReady))) << "known frame ready";
  EXPECT_TRUE(content::host_msg_name(content::HostMsg::kSharedHandle) ==
             "SharedHandle") << "name shared";
  EXPECT_TRUE(!content::is_known_host_msg(0)) << "reject 0";
  EXPECT_TRUE(!content::is_known_host_msg(0xFFF0)) << "reject high";
}

TEST(Ipc, BulkDataPipeOffer) {
  common::ipc::PendingRemote remote;
  common::ipc::PendingReceiver pending;
  EXPECT_TRUE(common::ipc::InterfaceEndpoint::create_pair(&remote, &pending)) << "bulk pair";
  common::ipc::Channel server = pending.take_channel();
  common::ipc::Channel client = remote.take_channel();
  client.set_peer_process(GetCurrentProcess());

  const std::string big(common::ipc::k_bulk_inline_max + 128, 'B');
  std::thread sender([&] {
    EXPECT_TRUE(common::ipc::send_bulk(&client, 42, 9, big)) << "bulk send";
  });

  common::ipc::Frame h;
  std::vector<uint8_t> payload;
  std::vector<common::ipc::PlatformHandle> handles;
  EXPECT_TRUE(server.recv(&h, &payload, &handles, 10000)) << "bulk recv offer";
  EXPECT_TRUE(h.type == common::ipc::k_msg_data_pipe_offer) << "bulk offer type";

  uint16_t original = 0;
  std::vector<uint8_t> out;
  EXPECT_TRUE(common::ipc::take_bulk_offer(h, payload, &handles, &original, &out)) << "bulk take";
  EXPECT_TRUE(original == 42) << "bulk original type";
  EXPECT_TRUE(out.size() == big.size() &&
             std::memcmp(out.data(), big.data(), big.size()) == 0) << "bulk bytes";
  sender.join();
}

TEST(Ipc, BrokerCreateSection) {
  common::ipc::PendingRemote remote;
  common::ipc::PendingReceiver pending;
  EXPECT_TRUE(common::ipc::InterfaceEndpoint::create_pair(&remote, &pending)) << "broker pair";
  common::ipc::Channel parent = pending.take_channel();
  common::ipc::Channel child = remote.take_channel();

  // Pseudo GetCurrentProcess() is fine as DuplicateHandle *target*, but keep a
  // real process handle for peer_ so Channel attachment wrap_into is explicit.
  HANDLE self = OpenProcess(PROCESS_DUP_HANDLE | SYNCHRONIZE, FALSE,
                            GetCurrentProcessId());
  EXPECT_TRUE(self != nullptr) << "broker OpenProcess self";
  parent.set_peer_process(self ? self : GetCurrentProcess());

  common::ipc::BrokerHost host;
  host.set_channel(&parent);
  host.set_client_process(self ? self : GetCurrentProcess());

  HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  std::thread pump([&] {
    SetEvent(ready);
    common::ipc::Frame h;
    std::vector<uint8_t> payload;
    std::vector<common::ipc::PlatformHandle> handles;
    if (!parent.recv(&h, &payload, &handles, 8000)) {
      EXPECT_TRUE(false) << "broker parent recv";
      return;
    }
    EXPECT_TRUE(host.try_handle(h, payload, &handles)) << "broker handle";
  });
  WaitForSingleObject(ready, 5000);

  common::ipc::PlatformHandle section;
  EXPECT_TRUE(common::ipc::broker_create_section(&child, 4096, &section, 8000)) << "broker create";
  EXPECT_TRUE(section.is_valid()) << "broker section valid";
  pump.join();
  if (ready) {
    CloseHandle(ready);
  }
  if (self) {
    CloseHandle(self);
  }
  section.reset();
}

