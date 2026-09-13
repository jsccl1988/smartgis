// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <asio.hpp>

#include "net/rpc/rpc.h"
#include "net/rpc/wire.h"

#include <chrono>

namespace net {

struct RpcClient::Impl {
  asio::io_context io;
  asio::ip::tcp::socket socket{io};
  asio::streambuf incoming;
  int timeout_ms = 5000;
  uint64_t next_uid = 1;
  bool connected = false;

  int wait_ms() const { return timeout_ms > 0 ? timeout_ms : 5000; }

  template <typename Start>
  bool wait_op(Start start, asio::error_code* out_ec) {
    asio::error_code op_ec = asio::error::would_block;
    bool done = false;
    start([&](const asio::error_code& e) {
      op_ec = e;
      done = true;
    });
    io.restart();
    io.run_for(std::chrono::milliseconds(wait_ms()));
    if (!done) {
      asio::error_code ignored;
      socket.cancel(ignored);
      io.run();
      if (out_ec) {
        *out_ec = asio::error::timed_out;
      }
      return false;
    }
    if (out_ec) {
      *out_ec = op_ec;
    }
    return !op_ec;
  }
};

RpcClient::RpcClient() : impl_(std::make_unique<Impl>()) {}

RpcClient::~RpcClient() {
  close();
}

void RpcClient::set_timeout_ms(int ms) {
  impl_->timeout_ms = ms > 0 ? ms : 5000;
}

void RpcClient::close() {
  asio::error_code ec;
  impl_->socket.close(ec);
  impl_->incoming.consume(impl_->incoming.size());
  impl_->connected = false;
}

bool RpcClient::connect(const RpcEndpoint& ep) {
  close();
  impl_->socket = asio::ip::tcp::socket(impl_->io);
  asio::error_code rec;
  asio::ip::tcp::resolver resolver(impl_->io);
  const auto results = resolver.resolve(asio::ip::tcp::v4(), ep.host,
                                        std::to_string(ep.port), rec);
  if (rec || results.empty()) {
    return false;
  }
  asio::error_code cec;
  const bool ok = impl_->wait_op(
      [&](auto done) {
        asio::async_connect(impl_->socket, results,
                             [done](const asio::error_code& e,
                                    const asio::ip::tcp::endpoint&) { done(e); });
      },
      &cec);
  impl_->connected = ok;
  return ok;
}

RpcResult RpcClient::call(const std::string& method, const std::string& json_in) {
  RpcResult out;
  if (!impl_->connected || !impl_->socket.is_open()) {
    out.error_code = detail::k_rpc_not_connected;
    out.error = "not connected";
    return out;
  }
  const uint64_t uid = impl_->next_uid++;
  const std::string frame =
      detail::line_encode(detail::encode_call_body(uid, method, json_in));

  asio::error_code wec;
  if (!impl_->wait_op(
          [&](auto done) {
            asio::async_write(impl_->socket, asio::buffer(frame),
                              [done](const asio::error_code& e, std::size_t) { done(e); });
          },
          &wec)) {
    out.error_code = detail::k_rpc_timeout;
    out.error = "timeout";
    return out;
  }

  asio::error_code rec;
  std::size_t nread = 0;
  if (!impl_->wait_op(
          [&](auto done) {
            asio::async_read_until(
                impl_->socket, impl_->incoming, '\n',
                [&, done](const asio::error_code& e, std::size_t n) {
                  nread = n;
                  done(e);
                });
          },
          &rec)) {
    out.error_code = detail::k_rpc_timeout;
    out.error = "timeout";
    return out;
  }
  if (nread == 0) {
    out.error_code = detail::k_rpc_internal;
    out.error = "bad frame";
    return out;
  }

  const auto bufs = impl_->incoming.data();
  std::string raw(asio::buffers_begin(bufs),
                  asio::buffers_begin(bufs) + static_cast<std::ptrdiff_t>(nread));
  impl_->incoming.consume(nread);
  const std::string_view body = detail::line_strip(raw);

  uint16_t err = 0;
  std::string json;
  if (!detail::decode_string_result_body(body, &err, &json)) {
    out.error_code = detail::k_rpc_internal;
    out.error = "bad frame";
    return out;
  }
  out.error_code = err;
  out.json = std::move(json);
  if (err != detail::k_rpc_ok) {
    out.error = "rpc error";
    return out;
  }
  out.ok = true;
  return out;
}

}  // namespace net
