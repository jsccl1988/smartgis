// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TRACE_DETAIL_JSON_APPEND_H_
#define BASE_TRACE_DETAIL_JSON_APPEND_H_

#include <cstdint>
#include <string>
#include <string_view>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace base {
namespace trace {
namespace detail {

// Append a JSON-escaped string fragment (no surrounding quotes) via RapidJSON.
inline void append_str(std::string* out, std::string_view s) {
  if (!out) {
    return;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.String(s.data(), static_cast<rapidjson::SizeType>(s.size()));
  const char* p = buf.GetString();
  const size_t n = buf.GetSize();
  if (n >= 2 && p[0] == '"' && p[n - 1] == '"') {
    out->append(p + 1, n - 2);
  } else {
    out->append(p, n);
  }
}

inline void append_i64(std::string* out, std::int64_t v) {
  if (!out) {
    return;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.Int64(v);
  out->append(buf.GetString(), buf.GetSize());
}

}  // namespace detail
}  // namespace trace
}  // namespace base

#endif  // BASE_TRACE_DETAIL_JSON_APPEND_H_
