// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <asio.hpp>

#include "net/udp/udp.h"

#include <string>

namespace net {
namespace {

asio::ip::udp::endpoint resolve_ep(asio::io_context& io, const std::string& host,
                                     uint16_t port, asio::error_code* ec) {
  if (host.empty()) {
    *ec = asio::error_code();
    return asio::ip::udp::endpoint(asio::ip::udp::v4(), port);
  }
  asio::error_code parse_ec;
  const auto addr = asio::ip::make_address_v4(host, parse_ec);
  if (!parse_ec) {
    *ec = asio::error_code();
    return asio::ip::udp::endpoint(addr, port);
  }
  asio::ip::udp::resolver resolver(io);
  const auto results =
      resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port), *ec);
  if (*ec || results.empty()) {
    return {};
  }
  *ec = asio::error_code();
  return *results.begin();
}

}  // namespace

struct UdpSocket::Impl {
  asio::io_context io;
  asio::ip::udp::socket sock{io};
};

UdpSocket::UdpSocket() : impl_(std::make_unique<Impl>()) {}

UdpSocket::~UdpSocket() {
  close();
}

bool UdpSocket::open() {
  close();
  asio::error_code ec;
  impl_->sock.open(asio::ip::udp::v4(), ec);
  return !ec;
}

void UdpSocket::close() {
  asio::error_code ec;
  if (impl_->sock.is_open()) {
    impl_->sock.close(ec);
  }
}

bool UdpSocket::bind(const std::string& host, uint16_t port) {
  if (!impl_->sock.is_open()) {
    return false;
  }
  asio::error_code ec;
  const auto ep = resolve_ep(impl_->io, host, port, &ec);
  if (ec) {
    return false;
  }
  impl_->sock.bind(ep, ec);
  return !ec;
}

bool UdpSocket::set_recv_buf(int bytes) {
  if (!impl_->sock.is_open()) {
    return false;
  }
  asio::error_code ec;
  impl_->sock.set_option(asio::socket_base::receive_buffer_size(bytes), ec);
  return !ec;
}

bool UdpSocket::local_endpoint(UdpEndpoint* ep) {
  if (ep == nullptr || !impl_->sock.is_open()) {
    return false;
  }
  asio::error_code ec;
  const auto local = impl_->sock.local_endpoint(ec);
  if (ec) {
    return false;
  }
  ep->host = local.address().to_string();
  ep->port = local.port();
  return true;
}

int UdpSocket::send_to(const void* buf, int len, const UdpEndpoint& to) {
  if (!impl_->sock.is_open() || buf == nullptr) {
    return -1;
  }
  asio::error_code ec;
  const auto ep = resolve_ep(impl_->io, to.host, to.port, &ec);
  if (ec) {
    return -1;
  }
  const auto n =
      impl_->sock.send_to(asio::buffer(buf, static_cast<size_t>(len)), ep, 0, ec);
  return ec ? -1 : static_cast<int>(n);
}

int UdpSocket::receive_from(void* buf, int len, UdpEndpoint* from) {
  if (!impl_->sock.is_open() || buf == nullptr) {
    return -1;
  }
  asio::ip::udp::endpoint ep;
  asio::error_code ec;
  const auto n =
      impl_->sock.receive_from(asio::buffer(buf, static_cast<size_t>(len)), ep, 0, ec);
  if (ec) {
    return -1;
  }
  if (from != nullptr) {
    from->host = ep.address().to_string();
    from->port = ep.port();
  }
  return static_cast<int>(n);
}

}  // namespace net
