// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_CODEC_H
#define BASE_IPC_CODEC_H

#include <cstddef>
#include <ios>
#include <sstream>
#include <streambuf>
#include <string>

#include "base/archive/archive.h"

namespace base {
namespace ipc {
namespace detail {

// Append into a contiguous string (same AppenderBuf pattern as net::Pickle).
struct StringWriteBuf : std::streambuf {
  std::string* s = nullptr;
  explicit StringWriteBuf(std::string* out) : s(out) { s->clear(); }
  std::streamsize xsputn(const char* p, std::streamsize n) override {
    s->append(p, static_cast<size_t>(n));
    return n;
  }
  int overflow(int c) override {
    using traits = std::char_traits<char>;
    if (traits::eq_int_type(c, traits::eof())) {
      return !traits::eof();
    }
    s->push_back(traits::to_char_type(c));
    return traits::not_eof(c);
  }
};

}  // namespace detail

// Encode/decode types with archive() or arithmetic/string (BinarySink).
template <typename T>
std::string encode(const T& value) {
  std::string buffer;
  detail::StringWriteBuf sbuf(&buffer);
  std::ostream os(&sbuf);
  Serializer<> ser(os);
  ser << value;
  return buffer;
}

template <typename T>
bool decode(const void* data, std::size_t n, T* out) {
  if (!out) {
    return false;
  }
  if (n > 0 && !data) {
    return false;
  }
  const char* bytes = n ? static_cast<const char*>(data) : "";
  std::stringstream ss(std::string(bytes, n), std::ios::in | std::ios::binary);
  Deserializer<> des(ss);
  des >> *out;
  return !ss.fail();
}

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_CODEC_H
