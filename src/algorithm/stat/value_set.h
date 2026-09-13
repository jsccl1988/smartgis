// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_STAT_VALUE_SET_H_
#define ALGORITHM_STAT_VALUE_SET_H_

#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include "base/core/core.h"

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(STAT_EXPORTS)
#define STAT_EXPORT_CLASS __declspec(dllexport)
#else
#define STAT_EXPORT_CLASS __declspec(dllimport)
#endif

namespace stat {

// Named GIS attribute / statistic columns used as expression operands.
class STAT_EXPORT_CLASS ValueSet {
 public:
  long bind(std::string name, std::span<const double> values);
  bool has(std::string_view name) const;
  std::span<double> get(std::string_view name);
  std::span<const double> get(std::string_view name) const;
  std::vector<std::string> names() const;
  void clear();

 private:
  std::map<std::string, std::vector<double>, std::less<>> values_;
};

}  // namespace stat

#endif  // ALGORITHM_STAT_VALUE_SET_H_
