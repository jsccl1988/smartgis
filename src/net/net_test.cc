// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <asio.hpp>
#include "httplib.h"

#include "net/http/http.h"
#include "net/pack/pickle.h"
#include "net/rpc/rpc.h"
#include "net/rpc/wire.h"

#include <cstdio>
#include <string>
#include <thread>
#include <utility>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void serve_rpc_frames(asio::ip::tcp::socket& peer, int frames) {
  asio::streambuf incoming;
  for (int i = 0; i < frames; ++i) {
    asio::error_code ec;
    const std::size_t n = asio::read_until(peer, incoming, '\n', ec);
    if (ec || n == 0) {
      return;
    }
    const auto bufs = incoming.data();
    std::string raw(asio::buffers_begin(bufs),
                    asio::buffers_begin(bufs) + static_cast<std::ptrdiff_t>(n));
    incoming.consume(n);
    uint64_t uid = 0;
    uint16_t err = 0;
    std::string method;
    std::string json;
    if (!net::detail::decode_call_body(net::detail::line_strip(raw), &uid, &err,
                                        &method, &json)) {
      return;
    }
    std::string body;
    if (method == "echo") {
      body = net::detail::encode_string_result_body(uid, json);
    } else {
      body = net::detail::encode_error_body(uid, net::detail::k_rpc_not_bound);
    }
    const std::string frame = net::detail::line_encode(std::move(body));
    asio::write(peer, asio::buffer(frame), ec);
  }
}

void test_rpc() {
  static_assert(net::rpc_transport_traits<net::AsioTcp>::supported,
                "asio tcp transport is enabled");
  static_assert(!net::rpc_transport_traits<int>::supported, "int is not a transport");

  {
    asio::io_context io;
    asio::ip::tcp::acceptor acc(io, {asio::ip::tcp::v4(), 0});
    const int port = static_cast<int>(acc.local_endpoint().port());
    acc.close();
    net::RpcClient rpc;
    rpc.set_timeout_ms(500);
    expect(!rpc.connect({"127.0.0.1", port}), "connect refused");
  }

  asio::io_context io;
  asio::ip::tcp::acceptor acc(io, {asio::ip::tcp::v4(), 0});
  const int port = static_cast<int>(acc.local_endpoint().port());
  std::thread th([&]() {
    asio::ip::tcp::socket peer(io);
    acc.accept(peer);
    serve_rpc_frames(peer, 2);
  });

  net::RpcClient rpc;
  expect(rpc.connect({"127.0.0.1", port}), "rpc connect");
  const net::RpcResult echo = rpc.call("echo", "{\"x\":1}");
  expect(echo.ok, "echo ok");
  expect(echo.json == "{\"x\":1}", "echo json");

  const net::RpcResult miss = rpc.call("infra.tabled.catalog.list", "{}");
  expect(!miss.ok, "unbound fails");
  expect(miss.error_code == net::detail::k_rpc_not_bound, "unbound code");

  rpc.close();
  th.join();
}

void test_pickle() {
  net::Pickle w;
  w << std::string("echo") << std::string("{\"x\":1}");
  const std::string bytes = w.extract_wire();
  net::Pickle r(bytes.data(), bytes.size());
  std::string method;
  std::string json;
  r >> method >> json;
  expect(method == "echo", "pickle method");
  expect(json == "{\"x\":1}", "pickle json");

  const std::string body = net::detail::encode_call_body(7, "echo", "{\"x\":1}");
  uint64_t uid = 0;
  uint16_t err = 99;
  std::string m2;
  std::string j2;
  expect(net::detail::decode_call_body(body, &uid, &err, &m2, &j2), "decode call");
  expect(uid == 7, "pickle uid");
  expect(err == 0, "pickle err");
  expect(m2 == "echo" && j2 == "{\"x\":1}", "pickle envelope");

  const std::string err_body =
      net::detail::encode_error_body(9, net::detail::k_rpc_timeout);
  uint16_t err2 = 0;
  std::string leftover = "keep";
  expect(net::detail::decode_string_result_body(err_body, &err2, &leftover),
         "decode error");
  expect(err2 == net::detail::k_rpc_timeout, "error code");
  expect(leftover.empty(), "error skips value");

  std::string oversize;
  const std::size_t n = base::k_binary_wire_max_string_bytes + 1;
  oversize.append(reinterpret_cast<const char*>(&n), sizeof(n));
  net::Pickle too_big(oversize.data(), oversize.size());
  std::string s = "keep";
  too_big >> s;
  expect(s.empty(), "oversize string rejected");
}

void test_http_loopback() {
  httplib::Server svr;
  svr.Get("/hello", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("ok", "text/plain");
  });
  const int port = svr.bind_to_any_port("127.0.0.1");
  expect(port > 0, "http bind");
  std::thread th([&svr]() { svr.listen_after_bind(); });
  svr.wait_until_ready();

  net::HttpClient cli;
  const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/hello";
  const net::HttpResult got = cli.get(url, 5);
  expect(got.ok, "http get ok");
  expect(got.status == 200, "http 200");
  expect(got.body == "ok", "http body");

  const net::HttpResult bad = cli.get("not-a-url", 1);
  expect(!bad.ok, "invalid url fails");

  svr.stop();
  th.join();
}

void test_https_scheme() {
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
  std::fprintf(stderr, "SKIP: https (CPPHTTPLIB_OPENSSL_SUPPORT unset)\n");
  return;
#else
  // Prove HTTPS Client construction path is live. Port 1 is closed → transport
  // failure (not "unsupported scheme"). No external network.
  net::HttpClient cli;
  cli.set_ssl_verify(false);
  const net::HttpResult got = cli.get("https://127.0.0.1:1/", 1);
  expect(!got.ok, "https closed port fails");
  expect(got.error != "invalid url", "https url parses");
#endif
}

}  // namespace

int main() {
  test_pickle();
  test_rpc();
  test_http_loopback();
  test_https_scheme();
  if (g_fails != 0) {
    std::fprintf(stderr, "net_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("net_test: ok\n");
  return 0;
}
