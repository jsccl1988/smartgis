// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/stat/detail/ops.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace stat::detail {
namespace {

bool resize_for_broadcast(std::vector<double>& left, std::size_t right_n) {
  if (right_n == 0 || left.empty()) {
    return false;
  }
  if (left.size() == 1 && right_n > 1) {
    left.assign(right_n, left[0]);
  }
  return left.size() == right_n || right_n == 1;
}

}  // namespace

bool combine(std::vector<double>& left, std::span<const double> right,
             double (*fn)(double, double)) {
  if (!resize_for_broadcast(left, right.size())) {
    return false;
  }
  if (right.size() == 1) {
    const double r = right[0];
    for (double& v : left) {
      v = fn(v, r);
    }
    return true;
  }
  for (std::size_t i = 0; i < left.size(); ++i) {
    left[i] = fn(left[i], right[i]);
  }
  return true;
}

void apply_unary(std::vector<double>& xs, double (*fn)(double)) {
  for (double& v : xs) {
    v = fn(v);
  }
}

double reduce_sum(std::span<const double> xs) {
  return std::accumulate(xs.begin(), xs.end(), 0.0);
}

double reduce_avg(std::span<const double> xs) {
  return xs.empty() ? 0.0 : reduce_sum(xs) / static_cast<double>(xs.size());
}

double reduce_min(std::span<const double> xs) {
  return *std::min_element(xs.begin(), xs.end());
}

double reduce_max(std::span<const double> xs) {
  return *std::max_element(xs.begin(), xs.end());
}

double reduce_median(std::span<const double> xs) {
  std::vector<double> tmp(xs.begin(), xs.end());
  const auto mid = tmp.size() / 2;
  std::nth_element(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(mid),
                   tmp.end());
  if (tmp.size() % 2 == 1) {
    return tmp[mid];
  }
  const double hi = tmp[mid];
  std::nth_element(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                   tmp.end());
  return (tmp[mid - 1] + hi) * 0.5;
}

double reduce_variance(std::span<const double> xs) {
  if (xs.size() < 2) {
    return 0.0;
  }
  const double mean = reduce_avg(xs);
  double acc = 0.0;
  for (double v : xs) {
    const double d = v - mean;
    acc += d * d;
  }
  return acc / static_cast<double>(xs.size());
}

double reduce_std_deviation(std::span<const double> xs) {
  return std::sqrt(reduce_variance(xs));
}

void std_by_sum(std::span<double> xs) {
  const double s = reduce_sum(xs);
  if (s == 0.0) {
    return;
  }
  for (double& v : xs) {
    v /= s;
  }
}

void std_by_max(std::span<double> xs) {
  const double m = reduce_max(xs);
  if (m == 0.0) {
    return;
  }
  for (double& v : xs) {
    v /= m;
  }
}

void std_by_min(std::span<double> xs) {
  const double m = reduce_min(xs);
  if (m == 0.0) {
    return;
  }
  for (double& v : xs) {
    v /= m;
  }
}

void std_by_max_min(std::span<double> xs) {
  const double lo = reduce_min(xs);
  const double hi = reduce_max(xs);
  const double span = hi - lo;
  if (span == 0.0) {
    return;
  }
  for (double& v : xs) {
    v = (v - lo) / span;
  }
}

void std_by_deviation(std::span<double> xs) {
  const double mean = reduce_avg(xs);
  const double sd = reduce_std_deviation(xs);
  if (sd == 0.0) {
    return;
  }
  for (double& v : xs) {
    v = (v - mean) / sd;
  }
}

std::string field_name(std::string_view token) {
  if (token.size() >= 2 && token.front() == '[' && token.back() == ']') {
    return std::string(token.substr(1, token.size() - 2));
  }
  return std::string(token);
}

}  // namespace stat::detail
