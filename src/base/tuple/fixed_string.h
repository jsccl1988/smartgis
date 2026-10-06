// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_TUPLE_FIXED_STRING_H
#define BASE_TUPLE_FIXED_STRING_H

#include <cstddef>
#include <string_view>

namespace base {
namespace tuple {

// NTTP compile-time string (mogu named-tuple field names).
template <std::size_t N>
struct fixed_string {
  char data[N]{};

  constexpr fixed_string(const char (&s)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
      data[i] = s[i];
    }
  }

  constexpr std::string_view view() const {
    return std::string_view(data, N ? N - 1 : 0);
  }

  constexpr const char* c_str() const { return data; }

  constexpr std::size_t size() const { return N ? N - 1 : 0; }
};

template <std::size_t N, std::size_t M>
constexpr bool operator==(const fixed_string<N>& a, const fixed_string<M>& b) {
  return a.view() == b.view();
}

}  // namespace tuple
}  // namespace base

#endif  // BASE_TUPLE_FIXED_STRING_H
