// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_RPC_H
#define SMT_NET_RPC_H

#include "net/net_export.h"

#include <cstdint>
#include <memory>
#include <string>

namespace net {

struct RpcEndpoint {
  std::string host{"127.0.0.1"};
  int port = 9030;
};

struct RpcResult {
  bool ok = false;
  uint16_t error_code = 0;
  std::string json;
  std::string error;
};

struct AsioTcp {};

template <typename Transport>
struct rpc_transport_traits {
  static constexpr bool supported = false;
};

template <>
struct rpc_transport_traits<AsioTcp> {
  static constexpr bool supported = true;
};

template <typename Transport>
concept rpc_transport = rpc_transport_traits<Transport>::supported;

// FnRPC client: ASIO TCP, tabled CRLF pickle, JSON string payload.
class NET_EXPORT RpcClient {
 public:
  RpcClient();
  ~RpcClient();

  RpcClient(const RpcClient&) = delete;
  RpcClient& operator=(const RpcClient&) = delete;

  bool connect(const RpcEndpoint& ep = {});
  void close();
  void set_timeout_ms(int ms);
  RpcResult call(const std::string& method, const std::string& json_in);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace net

#endif  // SMT_NET_RPC_H
