// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_STAT_DETAIL_OPS_H
#define SMT_STAT_DETAIL_OPS_H

#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stat::detail {

// Vectorized real-set ops. Size-1 values broadcast against a longer set.
bool combine(std::vector<double>& left, std::span<const double> right,
             double (*fn)(double, double));
void apply_unary(std::vector<double>& xs, double (*fn)(double));
double reduce_sum(std::span<const double> xs);
double reduce_avg(std::span<const double> xs);
double reduce_min(std::span<const double> xs);
double reduce_max(std::span<const double> xs);
double reduce_median(std::span<const double> xs);
double reduce_variance(std::span<const double> xs);
double reduce_std_deviation(std::span<const double> xs);
void std_by_sum(std::span<double> xs);
void std_by_max(std::span<double> xs);
void std_by_min(std::span<double> xs);
void std_by_max_min(std::span<double> xs);
void std_by_deviation(std::span<double> xs);
std::string field_name(std::string_view token);

}  // namespace stat::detail

#endif  // SMT_STAT_DETAIL_OPS_H
