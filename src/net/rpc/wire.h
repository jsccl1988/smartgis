// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_RPC_WIRE_H
#define SMT_NET_RPC_WIRE_H

#include "net/pack/pickle.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace net {
namespace detail {

inline constexpr uint16_t k_rpc_ok = 0;
inline constexpr uint16_t k_rpc_not_connected = 1;
inline constexpr uint16_t k_rpc_not_bound = 2;
inline constexpr uint16_t k_rpc_timeout = 3;
inline constexpr uint16_t k_rpc_internal = 4;
inline constexpr uint16_t k_rpc_invalid_arg = 5;

// mogu net::trace_t + head_t (default protocol tail: schema_version + extension).
struct RpcTrace {
  uint64_t trace_id = 0;
  uint64_t parent_id = 0;
  uint64_t span_id = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(trace_id, parent_id, span_id);
  }
};

struct RpcHead {
  uint64_t uid = 0;
  uint16_t error_code = 0;
  RpcTrace trace;
  uint32_t protocol = 0;
  uint32_t schema_version = 1;
  std::string extension;

  bool valid() const { return error_code == 0; }

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(uid, error_code, trace, protocol, schema_version, extension);
  }
};

// mogu message_t<string>: head, then value only when error_code == 0.
struct RpcMessage {
  RpcHead head;
  std::string value;
};

inline std::string line_encode(std::string payload) {
  while (!payload.empty() && (payload.back() == '\n' || payload.back() == '\r')) {
    payload.pop_back();
  }
  payload.append("\r\n");
  return payload;
}

inline std::string_view line_strip(std::string_view frame) {
  while (!frame.empty() && (frame.back() == '\n' || frame.back() == '\r')) {
    frame.remove_suffix(1);
  }
  return frame;
}

}  // namespace detail
}  // namespace net

namespace net {

template <>
struct binary_format_traits<detail::RpcMessage> {
  template <typename Stream>
  static void write(Stream&& stream, const detail::RpcMessage& value) {
    Serializer ar(stream);
    ar << value.head;
    if (value.head.valid()) {
      ar << value.value;
    }
  }

  template <typename Stream>
  static void read(Stream&& stream, detail::RpcMessage& value) {
    Deserializer ar(stream);
    ar >> value.head;
    if (value.head.valid()) {
      ar >> value.value;
    }
  }
};

namespace detail {

inline std::string encode_call_body(uint64_t uid, const std::string& method,
                                     const std::string& json) {
  Pickle inner;
  inner << method << json;
  RpcMessage msg;
  msg.head.uid = uid;
  msg.value = inner.extract_wire();
  Pickle outer;
  outer << msg;
  return outer.extract_wire();
}

inline bool decode_call_body(std::string_view body, uint64_t* uid, uint16_t* err,
                              std::string* method, std::string* json) {
  Pickle outer(body.data(), body.size());
  RpcMessage msg;
  outer >> msg;
  *uid = msg.head.uid;
  *err = msg.head.error_code;
  if (!msg.head.valid()) {
    method->clear();
    json->clear();
    return true;
  }
  Pickle inner(msg.value.data(), msg.value.size());
  inner >> *method >> *json;
  return true;
}

inline std::string encode_string_result_body(uint64_t uid, const std::string& json) {
  Pickle result;
  result << json;
  RpcMessage msg;
  msg.head.uid = uid;
  msg.value = result.extract_wire();
  Pickle outer;
  outer << msg;
  return outer.extract_wire();
}

inline std::string encode_error_body(uint64_t uid, uint16_t code) {
  RpcMessage msg;
  msg.head.uid = uid;
  msg.head.error_code = code;
  Pickle outer;
  outer << msg;
  return outer.extract_wire();
}

inline bool decode_string_result_body(std::string_view body, uint16_t* err,
                                       std::string* json) {
  Pickle outer(body.data(), body.size());
  RpcMessage msg;
  outer >> msg;
  *err = msg.head.error_code;
  if (!msg.head.valid()) {
    json->clear();
    return true;
  }
  Pickle inner(msg.value.data(), msg.value.size());
  inner >> *json;
  return true;
}

}  // namespace detail
}  // namespace net

#endif  // SMT_NET_RPC_WIRE_H
