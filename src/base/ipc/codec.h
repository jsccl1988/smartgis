// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_CODEC_H
#define BASE_IPC_CODEC_H

#include <cstddef>
#include <string>

#include "net/pack/pickle.h"

namespace base {
namespace ipc {

// Pickle a C++ type that has archive() or is arithmetic/string (mogu BinarySink).
template <typename T>
std::string encode(const T& value) {
  net::Pickle w;
  w << value;
  return w.extract_wire();
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
  net::Pickle r(bytes, n);
  r >> *out;
  return r.ok();
}

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_CODEC_H
