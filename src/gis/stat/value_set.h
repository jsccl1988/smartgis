// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_STAT_VALUE_VALUE_SET_H_
#define GIS_STAT_VALUE_VALUE_SET_H_


#include "gis/gis_export.h"
#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include "legacy/core/macros/macros.h"

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace stat {

// Named GIS attribute / statistic columns used as expression operands.
class GIS_EXPORT ValueSet {
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

#endif  // GIS_STAT_VALUE_VALUE_SET_H_
