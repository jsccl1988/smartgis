// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_UDP_H
#define SMT_NET_UDP_H

#include "net/net_export.h"

#include <cstdint>
#include <memory>
#include <string>

namespace net {

struct UdpEndpoint {
  std::string host;
  uint16_t port = 0;
};

// ASIO UDP datagram socket. No Winsock types on the public API.
class NET_EXPORT UdpSocket {
 public:
  UdpSocket();
  ~UdpSocket();

  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;

  bool open();
  void close();
  bool bind(const std::string& host, uint16_t port);
  bool set_recv_buf(int bytes);
  bool local_endpoint(UdpEndpoint* ep);

  int send_to(const void* buf, int len, const UdpEndpoint& to);
  int receive_from(void* buf, int len, UdpEndpoint* from);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace net

#endif  // SMT_NET_UDP_H
