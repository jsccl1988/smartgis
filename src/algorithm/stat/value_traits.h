// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_STAT_VALUE_TRAITS_H
#define SMT_STAT_VALUE_TRAITS_H

#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include <concepts>
#include <cstddef>
#include <span>
#include <vector>

namespace stat {

// Collapses pointer / vector / span ranges to a double span. Compile-time
// dimension does not apply; GIS value sets are runtime-length.
template <typename R>
struct value_traits;

template <>
struct value_traits<std::span<const double>> {
  using coordinate_type = double;
  static std::span<const double> as_span(std::span<const double> r) { return r; }
};

template <>
struct value_traits<std::span<double>> {
  using coordinate_type = double;
  static std::span<const double> as_span(std::span<double> r) { return r; }
};

template <>
struct value_traits<std::vector<double>> {
  using coordinate_type = double;
  static std::span<const double> as_span(const std::vector<double>& r) {
    return std::span<const double>(r);
  }
};

template <std::size_t N>
struct value_traits<double[N]> {
  using coordinate_type = double;
  static std::span<const double> as_span(const double (&r)[N]) {
    return std::span<const double>(r, N);
  }
};

template <typename R>
concept real_range = requires(const R& r) {
  typename value_traits<R>::coordinate_type;
  { value_traits<R>::as_span(r) } -> std::same_as<std::span<const double>>;
};

}  // namespace stat

#endif  // SMT_STAT_VALUE_TRAITS_H
