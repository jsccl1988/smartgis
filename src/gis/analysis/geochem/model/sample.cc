// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/geochem/model/sample.h"

#include <cmath>

namespace gis {
namespace detail {

int element_index(const GeochemSampleSet& set, std::string_view element) {
  for (size_t i = 0; i < set.element_names.size(); ++i) {
    if (set.element_names[i] == element) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool values_for_element(const GeochemSampleSet& set,
                        std::string_view element,
                        std::vector<double>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  const int idx = element_index(set, element);
  if (idx < 0) {
    return false;
  }
  out->reserve(set.samples.size());
  for (const GeochemSample& s : set.samples) {
    if (static_cast<size_t>(idx) >= s.values.size()) {
      continue;
    }
    const double v = s.values[static_cast<size_t>(idx)];
    if (std::isfinite(v)) {
      out->push_back(v);
    }
  }
  return !out->empty();
}

}  // namespace detail
}  // namespace gis
